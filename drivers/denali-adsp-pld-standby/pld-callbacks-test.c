#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint32_t u32;
typedef uint32_t __le32;
typedef uint64_t __le64;

#define __packed __attribute__((__packed__))
#define __maybe_unused __attribute__((__unused__))
#define READ_ONCE(x) (x)
#define cpu_to_le32(x) ((__le32)(x))
#define cpu_to_le64(x) ((__le64)(x))

enum { PM_SUSPEND_TO_IDLE = 1, PM_SUSPEND_MEM = 3 };

#define PLD_GLINK_MSG_SIZE		0x18U
#define PLD_GLINK_MSG_TYPE_MS_STATE	0x01U
#define PLD_GLINK_MSG_VERSION		0x01U
#define PLD_MODERN_STANDBY_EXIT		0x00U
#define PLD_MODERN_STANDBY_ENTER	0x01U

struct pld_glink_ms_state_msg {
	__le32 size;
	__le32 msg_type;
	__le32 version;
	__le32 modern_standby_state;
	__le64 reserved;
} __packed;
_Static_assert(sizeof(struct pld_glink_ms_state_msg) == 24, "packet must be 24 bytes");

struct rpmsg_endpoint {
	unsigned int sends;
	int next_error;
	struct pld_glink_ms_state_msg last_msg;
};

struct rpmsg_device {
	struct rpmsg_endpoint *ept;
};

struct denali_adsp_pld {
	struct rpmsg_device *rpdev;
	bool sleep_notified;
	unsigned int enter_count;
	unsigned int exit_count;
	unsigned int rx_count;
};

struct device {
	struct denali_adsp_pld *pld;
};

static bool denali_pld_sleep_notify;
static int pm_suspend_target_state;
static unsigned int sleep_calls;

static struct denali_adsp_pld *dev_get_drvdata(struct device *dev)
{
	return dev->pld;
}

static void usleep_range(unsigned long min, unsigned long max)
{
	assert(min > 0 && max >= min);
	sleep_calls++;
}

static int rpmsg_send(struct rpmsg_endpoint *ept, const void *data, int len)
{
	assert(ept != NULL);
	assert(len == sizeof(struct pld_glink_ms_state_msg));
	if (ept->next_error)
		return ept->next_error;
	memcpy(&ept->last_msg, data, sizeof(ept->last_msg));
	ept->sends++;
	return 0;
}

static int denali_adsp_pld_send_ms_state(struct denali_adsp_pld *pld, u32 state)
{
	struct pld_glink_ms_state_msg msg = {
		.size = cpu_to_le32(PLD_GLINK_MSG_SIZE),
		.msg_type = cpu_to_le32(PLD_GLINK_MSG_TYPE_MS_STATE),
		.version = cpu_to_le32(PLD_GLINK_MSG_VERSION),
		.modern_standby_state = cpu_to_le32(state),
		.reserved = cpu_to_le64(0),
	};

	if (!pld || !pld->rpdev || !pld->rpdev->ept)
		return -ENODEV;

	return rpmsg_send(pld->rpdev->ept, &msg, sizeof(msg));
}

static int __maybe_unused denali_adsp_pld_suspend_late(struct device *dev)
{
	struct denali_adsp_pld *pld = dev_get_drvdata(dev);
	int ret;

	if (!READ_ONCE(denali_pld_sleep_notify) || !pld ||
	    pm_suspend_target_state != PM_SUSPEND_MEM)
		return 0;
	if (pld->sleep_notified)
		return -EIO;

	ret = denali_adsp_pld_send_ms_state(pld, PLD_MODERN_STANDBY_ENTER);
	if (ret)
		return ret;

	pld->sleep_notified = true;
	pld->enter_count++;
	usleep_range(5000, 8000);
	return 0;
}

static int __maybe_unused denali_adsp_pld_resume_early(struct device *dev)
{
	struct denali_adsp_pld *pld = dev_get_drvdata(dev);
	int ret;

	if (!pld || !pld->sleep_notified)
		return 0;

	ret = denali_adsp_pld_send_ms_state(pld, PLD_MODERN_STANDBY_EXIT);
	if (!ret) {
		pld->sleep_notified = false;
		pld->exit_count++;
		usleep_range(2000, 4000);
	}

	return ret;
}

int main(void)
{
	struct rpmsg_endpoint ept = { 0 };
	struct rpmsg_device rpdev = { .ept = &ept };
	struct denali_adsp_pld pld = { .rpdev = &rpdev };
	struct device dev = { .pld = &pld };

	/* 1. Default off (denali_pld_sleep_notify == false) */
	pm_suspend_target_state = PM_SUSPEND_MEM;
	assert(denali_adsp_pld_suspend_late(&dev) == 0);
	assert(!pld.sleep_notified && ept.sends == 0 && sleep_calls == 0);
	assert(denali_adsp_pld_resume_early(&dev) == 0);
	assert(ept.sends == 0);

	/* 2. Armed, but non-PM_SUSPEND_MEM target */
	denali_pld_sleep_notify = true;
	pm_suspend_target_state = PM_SUSPEND_TO_IDLE;
	assert(denali_adsp_pld_suspend_late(&dev) == 0);
	assert(!pld.sleep_notified && ept.sends == 0);

	/* 3. Send error in suspend_late does not set sleep_notified */
	pm_suspend_target_state = PM_SUSPEND_MEM;
	ept.next_error = -ETIMEDOUT;
	assert(denali_adsp_pld_suspend_late(&dev) == -ETIMEDOUT);
	assert(!pld.sleep_notified && ept.sends == 0);
	ept.next_error = 0;

	/* 4. Normal suspend_late -> resume_early cycle */
	assert(denali_adsp_pld_suspend_late(&dev) == 0);
	assert(pld.sleep_notified && pld.enter_count == 1 && ept.sends == 1 && sleep_calls == 1);
	assert(ept.last_msg.size == 0x18 && ept.last_msg.msg_type == 1 &&
	       ept.last_msg.version == 1 && ept.last_msg.modern_standby_state == 1 &&
	       ept.last_msg.reserved == 0);

	/* Duplicate suspend_late rejected with -EIO */
	assert(denali_adsp_pld_suspend_late(&dev) == -EIO);
	assert(ept.sends == 1);

	/* Disarming parameter before resume_early still restores PLD timer */
	denali_pld_sleep_notify = false;
	ept.next_error = -EBUSY;
	assert(denali_adsp_pld_resume_early(&dev) == -EBUSY);
	assert(pld.sleep_notified && pld.exit_count == 0);
	ept.next_error = 0;

	assert(denali_adsp_pld_resume_early(&dev) == 0);
	assert(!pld.sleep_notified && pld.exit_count == 1 && ept.sends == 2 && sleep_calls == 2);
	assert(ept.last_msg.size == 0x18 && ept.last_msg.msg_type == 1 &&
	       ept.last_msg.version == 1 && ept.last_msg.modern_standby_state == 0 &&
	       ept.last_msg.reserved == 0);

	puts("PLD_CALLBACKS_TEST_OK");
	return 0;
}
