# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Espressif/frameworks/esp-idf-v5.2.2/components/bootloader/subproject"
  "E:/Nowe_kodowanie/esp-idf/zadanie_blu/esp_hid_device/build/bootloader"
  "E:/Nowe_kodowanie/esp-idf/zadanie_blu/esp_hid_device/build/bootloader-prefix"
  "E:/Nowe_kodowanie/esp-idf/zadanie_blu/esp_hid_device/build/bootloader-prefix/tmp"
  "E:/Nowe_kodowanie/esp-idf/zadanie_blu/esp_hid_device/build/bootloader-prefix/src/bootloader-stamp"
  "E:/Nowe_kodowanie/esp-idf/zadanie_blu/esp_hid_device/build/bootloader-prefix/src"
  "E:/Nowe_kodowanie/esp-idf/zadanie_blu/esp_hid_device/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/Nowe_kodowanie/esp-idf/zadanie_blu/esp_hid_device/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/Nowe_kodowanie/esp-idf/zadanie_blu/esp_hid_device/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
