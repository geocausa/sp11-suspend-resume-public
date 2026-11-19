// SPDX-License-Identifier: GPL-2.0-only
/*
 * Surface Pro 11 (Denali OLED) ADSP Power Limit Driver (PLD) Modern Standby
 * notification client over GLINK (APPS_ADSP_PWR_LMTS_GLINK_PORT).
 */

#include <linux/build_bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/rpmsg.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/types.h>

#define PLD_GLINK_CHANNEL_NAME		"APPS_ADSP_PWR_LMTS_GLINK_PORT"
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
static_assert(sizeof(struct pld_glink_ms_state_msg) == PLD_GLINK_MSG_SIZE);

struct denali_adsp_pld {
	struct rpmsg_device *rpdev;
	bool sleep_notified;
	unsigned int enter_count;
	unsigned int exit_count;
	unsigned int rx_count;
};

static bool denali_pld_sleep_notify;
module_param(denali_pld_sleep_notify, bool, 0600);
MODULE_PARM_DESC(denali_pld_sleep_notify,
		 "Send the Denali ADSP PLD Modern Standby state over GLINK during deep suspend");

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

static int denali_adsp_pld_callback(struct rpmsg_device *rpdev, void *data,
				    int len, void *priv, u32 src)
{
	struct denali_adsp_pld *pld = dev_get_drvdata(&rpdev->dev);

	if (pld)
		pld->rx_count++;

	dev_dbg(&rpdev->dev, "PLD GLINK rx len=%d src=0x%x\n", len, src);
	return 0;
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

	/*
	 * Allow ADSP pld_gport_read_handler to clear the PLD timer and return
	 * GLINK_CMD_RX_DONE_W_REUSE while normal device IRQs are still active
	 * and before parent 6800000.remoteproc asserts sleepstate bit 12.
	 */
	usleep_range(5000, 8000);
	return 0;
}

static int __maybe_unused denali_adsp_pld_resume_early(struct device *dev)
{
	struct denali_adsp_pld *pld = dev_get_drvdata(dev);
	int ret;

	if (!pld || !pld->sleep_notified)
		return 0;

	/* Restore the PLD timer even if the parameter was disarmed during sleep. */
	ret = denali_adsp_pld_send_ms_state(pld, PLD_MODERN_STANDBY_EXIT);
	if (!ret) {
		pld->sleep_notified = false;
		pld->exit_count++;
		usleep_range(2000, 4000);
	}

	return ret;
}

static int denali_adsp_pld_probe(struct rpmsg_device *rpdev)
{
	struct denali_adsp_pld *pld;

	if (!of_machine_is_compatible("microsoft,denali-oled"))
		return -ENODEV;
	if (!strstr(dev_name(&rpdev->dev), "6800000.remoteproc"))
		return -ENODEV;
	if (!rpdev->ept)
		return -ENODEV;

	pld = devm_kzalloc(&rpdev->dev, sizeof(*pld), GFP_KERNEL);
	if (!pld)
		return -ENOMEM;

	pld->rpdev = rpdev;
	dev_set_drvdata(&rpdev->dev, pld);

	dev_info(&rpdev->dev, "bound Denali ADSP PLD Modern Standby GLINK client\n");
	return 0;
}

static void denali_adsp_pld_remove(struct rpmsg_device *rpdev)
{
	struct denali_adsp_pld *pld = dev_get_drvdata(&rpdev->dev);

	if (pld && pld->sleep_notified) {
		if (!denali_adsp_pld_send_ms_state(pld, PLD_MODERN_STANDBY_EXIT))
			pld->sleep_notified = false;
	}
}

static const struct dev_pm_ops denali_adsp_pld_pm_ops = {
	.suspend_late = pm_sleep_ptr(denali_adsp_pld_suspend_late),
	.resume_early = pm_sleep_ptr(denali_adsp_pld_resume_early),
};

static const struct rpmsg_device_id denali_adsp_pld_id_table[] = {
	{ .name = PLD_GLINK_CHANNEL_NAME },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, denali_adsp_pld_id_table);

static struct rpmsg_driver denali_adsp_pld_driver = {
	.drv = {
		.name = "denali_adsp_pld_standby",
		.pm = pm_sleep_ptr(&denali_adsp_pld_pm_ops),
	},
	.id_table = denali_adsp_pld_id_table,
	.probe = denali_adsp_pld_probe,
	.remove = denali_adsp_pld_remove,
	.callback = denali_adsp_pld_callback,
};
module_rpmsg_driver(denali_adsp_pld_driver);

MODULE_DESCRIPTION("Surface Pro 11 Denali ADSP PLD Modern Standby GLINK client");
MODULE_LICENSE("GPL");
