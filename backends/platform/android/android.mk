# Android specific build targets

# These must be incremented for each market upload
#ANDROID_VERSIONCODE = 6  Specified in dists/android/AndroidManifest.xml.in
ANDROID_PLUGIN_VERSIONCODE = 6

JAVA_FILES = \
	ScummVM.java \
	ScummVMApplication.java \
	ScummVMActivity.java \
	EditableSurfaceView.java \
	Unpacker.java

JAVA_FILES_PLUGIN = \
	PluginProvider.java

JAVA_FILES_GEN = \
	Manifest.java \
	R.java

PATH_DIST = $(srcdir)/dists/android
PATH_RESOURCES = $(PATH_DIST)/res

RESOURCES = \
	$(PATH_RESOURCES)/values/strings.xml \
	$(PATH_RESOURCES)/layout/main.xml \
	$(PATH_RESOURCES)/layout/splash.xml \
	$(PATH_RESOURCES)/drawable/gradient.xml \
	$(PATH_RESOURCES)/drawable/scummvm.png \
	$(PATH_RESOURCES)/drawable/scummvm_big.png

PLUGIN_RESOURCES = \
	$(PATH_RESOURCES)/values/strings.xml \
	$(PATH_RESOURCES)/drawable/scummvm.png

# FIXME: find/mark plugin entry points and add all this back again:
#LDFLAGS += -Wl,--gc-sections
#CXXFLAGS += -ffunction-sections -fdata-sections -fvisibility=hidden -fvisibility-inlines-hidden

AAPT = $(ANDROID_SDK)/platform-tools/aapt
ADB = $(ANDROID_SDK)/platform-tools/adb
DX = $(ANDROID_SDK)/platform-tools/dx
APKBUILDER = $(ANDROID_SDK)/tools/apkbuilder
JAVAC ?= javac
JAVACFLAGS = -source 1.5 -target 1.5

# This is a bit silly.  I want to compile against the 1.6 android.jar,
# to make the compiler check that I don't use something that requires
# a newer Android.  However, in order to use android:installLocation,
# we need to give aapt a version >=8 android.jar - even though the
# result will work ok on 1.5+.
ANDROID_JAR = $(ANDROID_SDK)/platforms/android-4/android.jar
ANDROID_JAR8 = $(ANDROID_SDK)/platforms/android-8/android.jar

PATH_BUILD = build.tmp
PATH_BUILD_ASSETS = $(PATH_BUILD)/assets
PATH_BUILD_CLASSES_MAIN_TOP = $(PATH_BUILD)/classes.main
PATH_BUILD_CLASSES_PLUGIN_TOP = $(PATH_BUILD)/classes.plugin

PATH_STAGE_PREFIX = build.stage
PATH_STAGE_MAIN = $(PATH_STAGE_PREFIX).main

PATH_REL = org/inodes/gus/scummvm
PATH_SRC_TOP = $(srcdir)/backends/platform/android
PATH_SRC = $(PATH_SRC_TOP)/$(PATH_REL)

PATH_GEN_TOP = $(PATH_BUILD)/java
PATH_GEN = $(PATH_GEN_TOP)/$(PATH_REL)
PATH_CLASSES_MAIN = $(PATH_BUILD_CLASSES_MAIN_TOP)/$(PATH_REL)
PATH_CLASSES_PLUGIN = $(PATH_BUILD_CLASSES_PLUGIN_TOP)/$(PATH_REL)

FILE_MANIFEST = $(srcdir)/dists/android/AndroidManifest.xml
FILE_DEX = $(PATH_BUILD)/classes.dex
FILE_DEX_PLUGIN = $(PATH_BUILD)/plugins/classes.dex
FILE_RESOURCES = resources.ap_
FILE_RESOURCES_MAIN = $(PATH_BUILD)/$(FILE_RESOURCES)

SRC_GEN = $(addprefix $(PATH_GEN)/, $(JAVA_FILES_GEN))

CLASSES_MAIN = $(addprefix $(PATH_CLASSES_MAIN)/, $(JAVA_FILES:%.java=%.class))
CLASSES_GEN = $(addprefix $(PATH_CLASSES_MAIN)/, $(JAVA_FILES_GEN:%.java=%.class))
CLASSES_PLUGIN = $(addprefix $(PATH_CLASSES_PLUGIN)/, $(JAVA_FILES_PLUGIN:%.java=%.class))

APK_MAIN = scummvm.apk
APK_PLUGINS = $(patsubst plugins/lib%.so, scummvm-engine-%.apk, $(PLUGINS))

$(SRC_GEN): $(FILE_MANIFEST) $(filter %.xml,$(RESOURCES)) $(ANDROID_JAR8)
	@$(MKDIR) -p $(PATH_GEN_TOP)
	$(AAPT) package -m -J $(PATH_GEN_TOP) -M $< -S $(PATH_RESOURCES) -I $(ANDROID_JAR8)

$(PATH_CLASSES_MAIN)/%.class: $(PATH_GEN)/%.java $(SRC_GEN)
	@$(MKDIR) -p $(@D)
	$(JAVAC) $(JAVACFLAGS) -cp $(PATH_SRC_TOP) -d $(PATH_BUILD_CLASSES_MAIN_TOP) -bootclasspath $(ANDROID_JAR) $<

$(PATH_CLASSES_MAIN)/%.class: $(PATH_SRC)/%.java $(SRC_GEN)
	@$(MKDIR) -p $(@D)
	$(JAVAC) $(JAVACFLAGS) -cp $(PATH_SRC_TOP):$(PATH_GEN_TOP) -d $(PATH_BUILD_CLASSES_MAIN_TOP) -bootclasspath $(ANDROID_JAR) $<

$(PATH_CLASSES_PLUGIN)/%.class: $(PATH_SRC)/%.java
	@$(MKDIR) -p $(@D)
	$(JAVAC) $(JAVACFLAGS) -cp $(PATH_SRC_TOP) -d $(PATH_BUILD_CLASSES_PLUGIN_TOP) -bootclasspath $(ANDROID_JAR) $<

$(FILE_DEX): $(CLASSES_MAIN) $(CLASSES_GEN)
	$(DX) --dex --output=$@ $(PATH_BUILD_CLASSES_MAIN_TOP)

$(FILE_DEX_PLUGIN): $(CLASSES_PLUGIN)
	@$(MKDIR) -p $(@D)
	$(DX) --dex --output=$@ $(PATH_BUILD_CLASSES_PLUGIN_TOP)

$(PATH_BUILD)/%/AndroidManifest.xml $(PATH_STAGE_PREFIX).%/res/values/strings.xml: $(PATH_DIST)/mkmanifest.pl $(srcdir)/configure $(PATH_DIST)/AndroidManifest.xml
	$(PATH_DIST)/mkmanifest.pl --id=$* --configure=$(srcdir)/configure \
		--version-name=$(VERSION) \
		--version-code=$(ANDROID_PLUGIN_VERSIONCODE) \
		--stringres=$(PATH_STAGE_PREFIX).$*/res/values/strings.xml \
		--manifest=$(PATH_BUILD)/$*/AndroidManifest.xml \
		--master-manifest=$(PATH_DIST)/AndroidManifest.xml \
		--unpacklib=mylib/armeabi/lib$*.so

$(PATH_STAGE_PREFIX).%/res/drawable/scummvm.png: $(PATH_RESOURCES)/drawable/scummvm.png
	@$(MKDIR) -p $(@D)
	$(CP) $< $@

$(FILE_RESOURCES_MAIN): $(FILE_MANIFEST) $(RESOURCES) $(ANDROID_JAR8) $(DIST_FILES_THEMES) $(DIST_FILES_ENGINEDATA)
	$(INSTALL) -d $(PATH_BUILD_ASSETS)
	$(INSTALL) -c -m 644 $(DIST_FILES_THEMES) $(DIST_FILES_ENGINEDATA) $(PATH_BUILD_ASSETS)/
	$(AAPT) package -f -M $< -S $(PATH_RESOURCES) -A $(PATH_BUILD_ASSETS) -I $(ANDROID_JAR8) -F $@

$(PATH_BUILD)/%/$(FILE_RESOURCES): $(PATH_BUILD)/%/AndroidManifest.xml $(PATH_STAGE_PREFIX).%/res/values/strings.xml $(PATH_STAGE_PREFIX).%/res/drawable/scummvm.png plugins/lib%.so $(ANDROID_JAR8)
	$(AAPT) package -f -M $< -S $(PATH_STAGE_PREFIX).$*/res -I $(ANDROID_JAR8) -F $@

# Package installer won't delete old libscummvm.so on upgrade so
# replace it with a zero size file
$(APK_MAIN): $(EXECUTABLE) $(FILE_RESOURCES_MAIN) $(FILE_DEX)
	$(INSTALL) -d $(PATH_STAGE_MAIN)/common/lib/armeabi
	touch $(PATH_STAGE_MAIN)/common/lib/armeabi/libscummvm.so
	$(INSTALL) -d $(PATH_STAGE_MAIN)/common/mylib/armeabi
	$(INSTALL) -c -m 644 libscummvm.so $(PATH_STAGE_MAIN)/common/mylib/armeabi/
	$(STRIP) $(PATH_STAGE_MAIN)/common/mylib/armeabi/libscummvm.so
	$(APKBUILDER) $@ -z $(FILE_RESOURCES_MAIN) -f $(FILE_DEX) -rf $(PATH_STAGE_MAIN)/common || { $(RM) $@; exit 1; }

scummvm-engine-%.apk: plugins/lib%.so $(PATH_BUILD)/%/$(FILE_RESOURCES) $(FILE_DEX_PLUGIN)
	$(INSTALL) -d $(PATH_STAGE_PREFIX).$*/apk/mylib/armeabi/
	$(INSTALL) -c -m 644 plugins/lib$*.so $(PATH_STAGE_PREFIX).$*/apk/mylib/armeabi/
	$(STRIP) $(PATH_STAGE_PREFIX).$*/apk/mylib/armeabi/lib$*.so
	$(APKBUILDER) $@ -z $(PATH_BUILD)/$*/$(FILE_RESOURCES) -f $(FILE_DEX_PLUGIN) -rf $(PATH_STAGE_PREFIX).$*/apk || { $(RM) $@; exit 1; }

all: $(APK_MAIN) $(APK_PLUGINS)

clean: androidclean

androidclean:
	@$(RM) -rf $(PATH_BUILD) $(PATH_STAGE_PREFIX).* *.apk release

# remove debugging signature
release/%.apk: %.apk
	@$(MKDIR) -p $(@D)
	@$(RM) $@
	$(CP) $< $@.tmp
	zip -d $@.tmp META-INF/\*
	jarsigner $(JARSIGNER_FLAGS) $@.tmp release
	zipalign 4 $@.tmp $@
	$(RM) $@.tmp

androidrelease: $(addprefix release/, $(APK_MAIN) $(APK_PLUGINS))

androidtest: $(APK_MAIN) $(APK_PLUGINS)
	@set -e; for apk in $^; do \
		$(ADB) install -r $$apk; \
	done
	$(ADB) shell am start -a android.intent.action.MAIN -c android.intent.category.LAUNCHER -n org.inodes.gus.scummvm/.Unpacker

# used by buildbot!
androiddistdebug: all
	$(MKDIR) debug
	$(CP) $(APK_MAIN) $(APK_PLUGINS) debug/
	for i in $(DIST_FILES_DOCS); do \
		sed 's/$$/\r/' < $$i > debug/`basename $$i`.txt; \
	done

.PHONY: androidrelease androidtest

