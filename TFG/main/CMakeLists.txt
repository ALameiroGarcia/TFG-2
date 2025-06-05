# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS main.c wifi_ap.c web_server.c as7265x.c thingsboard_control.c oled.c# list the source files of this component
    INCLUDE_DIRS "." "../include"    # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES # optional, list the public requirements (component names)
    PRIV_REQUIRES   # optional, list the private requirements
)