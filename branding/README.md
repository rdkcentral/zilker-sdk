# Branding Build

## CMake

Builds a tar of files not in assets or res directory for the 
paritcular brand.  Also creates a header file with the list
of brands.

### Build-time installation
To build special defaults for a BUILD_MODEL, create a models/ tree
in the target namespace. For example, if you want a model specific
security/foo.txt, create a models/<BUILD_MODEL>/foo.txt in security.
The output brand will have the foo.txt from that model, if it exists,
overwriting any default in "model"'s parent. By default, you should touch
an _IGNORE_ file in 'models' to prevent `tar`ing them into the brand
defaults.

### Brand out-of-band specials
The environment, BUILD_BRAND_OOB (see the local CMakeLists.txt),
sets special tree roots to include in staging.
Currently there are two specials declared: fsroot and mirror.
fsroot is for baking files into a rootfs image in a recipe, and mirror
will install branded files into the icontrol home tree.

NB: Generally, anything you put into the mirror special will be installed
automatically by buildTools/functions.sh#createMiniMirror. The fsroot
special has no standard installation, so recipes will need to customize their
rootfs builds as needed.

## Gradle

Builds 2 APKs for each brand:
* icontrol_res.jar - Located in /vendor/jars, contains the 
compiled R.java file for framework resources, the BrandingEnum
class, BrandingAppEnum class, and all the AppStrings files.
This is roughly equivalent to a combination of the l10nLib and
icontrol_res from legacy.
* icontrol-res.apk - Located in /vendor/framework, contains
all the common brand resources, just like legacy.  Building
this requires using the custom AAPT that created as part of
our android OS build.

Unfortunately gradle requires newer version of the Android
build tools, but the version of aapt there seems to treat
our framework resource references from XML of the form 
```@*com.icontrol.branding:drawable/foo``` as AAR references.
This causes failures at runtime as it can't locate the 
referenced resources.  Resources referenced in code through
their ```R.foo``` constants works however.  

For workflow apps
this is not an issue, since they don't use Android XML, but
for apps what we are doing is just placing links to any 
referenced resources inside the app.  This means these 
resources can't be branded, but they were never really 
branded anyways, so its not a big deal.
