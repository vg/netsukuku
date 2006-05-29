include $(TOPDIR)/rules.mk

PKG_NAME:=netsukuku
PKG_VERSION:=0.0.7b
PKG_RELEASE:=1
PKG_MD5SUM:=1a61d2efe15f3114818cd9ef02fcb939

PKG_SOURCE_URL:=http://netsukuku.freaknet.org/files/
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2
PKG_CAT:=bzcat
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
PKG_INSTALL_DIR:=$(PKG_BUILD_DIR)/ipkg-install

include $(TOPDIR)/package/rules.mk

$(eval $(call PKG_template,NETSUKUKU,$(PKG_NAME),$(PKG_VERSION)-$(PKG_RELEASE),$(ARCH)))

PKG_DEPEND:="libopenssl, libgmp, kmod-tun, libpthread"
$(PKG_BUILD_DIR)/.configured: $(PKG_BUILD_DIR)/.prepared
	(cd $(PKG_BUILD_DIR); \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS)" \
		CPPFLAGS="-I$(STAGING_DIR)/usr/include -I$(STAGING_DIR)/include" \
		LDFLAGS="-L$(STAGING_DIR)/usr/lib -L$(STAGING_DIR)/lib" \
		./configure \
			--target=$(GNU_TARGET_NAME) \
			--host=$(GNU_TARGET_NAME) \
			--build=$(GNU_HOST_NAME) \
			--prefix=/usr \
			--without-libiconv-prefix \
			--without-libintl-prefix \
			--disable-nls \
	);
	touch $(PKG_BUILD_DIR)/.configured

$(PKG_BUILD_DIR)/.built: $(PKG_BUILD_DIR)/.configured
	$(MAKE) -C $(PKG_BUILD_DIR)
	mkdir -p $(PKG_INSTALL_DIR)
	$(MAKE) -C $(PKG_BUILD_DIR) \
		DESTDIR="$(PKG_INSTALL_DIR)" \
		install
	touch $(PKG_BUILD_DIR)/.built

$(IPKG_NETSUKUKU):
	install -d -m0755 $(IDIR_NETSUKUKU)/usr/bin
	cp -fpR $(PKG_INSTALL_DIR)/usr/bin/netsukuku_* $(IDIR_NETSUKUKU)/usr/bin
	install -d -m0755 $(IDIR_NETSUKUKU)/etc/netsukuku
	cp -fpR $(PKG_INSTALL_DIR)/etc/netsukuku/* $(IDIR_NETSUKUKU)/etc/netsukuku/
	$(RSTRIP) $(IDIR_NETSUKUKU)
	echo "Depends: $(PKG_DEPEND)" >> $(IDIR_NETSUKUKU)/CONTROL/control
	$(IPKG_BUILD) $(IDIR_NETSUKUKU) $(PACKAGE_DIR)

mostlyclean:
	make -C $(PKG_BUILD_DIR) clean
	rm $(PKG_BUILD_DIR)/.built
