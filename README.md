# mintrayr
Mozilla extension: Minimize windows into the system tray (Firefox, Thunderbird, Seamonkey, Instantbird)

# Developing the extension

Once you jumped through the hoops of getting a Firefox or Thunderbird set up for development, you can use a [proxy file](https://developer.mozilla.org/en-US/Add-ons/Setting_up_extension_development_environment#Firefox_extension_proxy_file) pointing to your cloned repository.

Most code is Javascript + some XUL and some CSS. But the low-level glue code had to be platform-native, meaning there is an dedicated implementation for Windows (a DLL) and one for linux (a shared object).

Please note that while the the Windows implementation uses C++, it does in fact not link the MS C/C++ runtime, so stuff like exceptions won't probably really work, and `new`/`delete` are implemented with calls to `LocalAlloc`. This is to avoid statically linking the massive runtime, and in fact, to avoid having two runtimes executing within the Firefox processs at the same time.

# Building an XPI

You need to have python-2 installed and then execute `python build.py` in the root directory of your clone. This will automatically generate an unsigned package.
Regarding the now-mandatory-in-Firefox add-on signing: You're on your own and have to figure that out yourself.
