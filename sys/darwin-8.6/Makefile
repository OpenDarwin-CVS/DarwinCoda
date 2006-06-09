TARGET           = coda_kext
TARGET_BUILD_DIR = /tmp/$(TARGET).dst/System/Library/Extensions
KEXTLOAD_FLAGS   = -v 6
FILTER           = grep -v -e "^    setenv "
# FILTER           = cat
XCODE            = xcodebuild -activeconfiguration

all:
	$(XCODE) build 2>&1 | $(FILTER)


install:
	$(XCODE) install 2>&1 | $(FILTER)

clean:
	$(XCODE) clean 2>&1 | $(FILTER)

test:
	@sync
	( cd $(TARGET_BUILD_DIR); kextload $(KEXTLOAD_FLAGS) darwin_coda.kext )

unload:
	@sync
	( cd $(TARGET_BUILD_DIR); kextunload darwin_coda.kext )
