#
# This file is the r5ctrlr-scpi-serv recipe.
#
# lines for sysV are commented: we use systemd
# so we do not need to put a start/stop script in /etc/init.d
#

SUMMARY = "R5 controller SCPI server"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://r5ctrlr-scpi-serv \
		file://r5ctrlr-scpi-serv-init \
		file://r5ctrlr-scpi-serv.service \
	"

S = "${WORKDIR}"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
  
inherit update-rc.d systemd

INITSCRIPT_NAME = "r5ctrlr-scpi-serv-init"
INITSCRIPT_PARAMS = "start 99 S ."
  
SYSTEMD_PACKAGES = "${PN}"
SYSTEMD_SERVICE:${PN} = "r5ctrlr-scpi-serv.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
RDEPENDS:${PN}:append = "libmetal open-amp"
#INSANE_SKIP:${PN}:append = "arch"

do_install() {
	        if ${@bb.utils.contains('DISTRO_FEATURES', 'sysvinit', 'true', 'false', d)}; then
                install -d ${D}${sysconfdir}/init.d/
                install -m 0755 ${WORKDIR}/r5ctrlr-scpi-serv-init ${D}${sysconfdir}/init.d/
			fi

	     install -d ${D}/${bindir}
	     install -m 0755 ${S}/r5ctrlr-scpi-serv ${D}/${bindir}
	     install -d ${D}${systemd_system_unitdir}
         install -m 0644 ${WORKDIR}/r5ctrlr-scpi-serv.service ${D}${systemd_system_unitdir}
}

FILES:${PN} += "${@bb.utils.contains('DISTRO_FEATURES','sysvinit','${sysconfdir}/*', '', d)}"
