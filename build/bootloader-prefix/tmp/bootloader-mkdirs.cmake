# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/ori88/esp/esp-idf/components/bootloader/subproject"
  "C:/esp/esp-idf/projects/smart-mails/build/bootloader"
  "C:/esp/esp-idf/projects/smart-mails/build/bootloader-prefix"
  "C:/esp/esp-idf/projects/smart-mails/build/bootloader-prefix/tmp"
  "C:/esp/esp-idf/projects/smart-mails/build/bootloader-prefix/src/bootloader-stamp"
  "C:/esp/esp-idf/projects/smart-mails/build/bootloader-prefix/src"
  "C:/esp/esp-idf/projects/smart-mails/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/esp/esp-idf/projects/smart-mails/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/esp/esp-idf/projects/smart-mails/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
