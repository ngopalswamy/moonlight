# - Try to find GStreamer and its plugins
# Once done, this will define
#
#  GSTREAMER_FOUND - system has GStreamer
#  GSTREAMER_INCLUDE_DIRS - the GStreamer include directories
#  GSTREAMER_LIBRARIES - link these to use GStreamer

# Find dependencies
set(THREADS_PREFER_PTHREAD_FLAG YES)
FIND_PACKAGE(Threads REQUIRED)
FIND_PACKAGE(GLib COMPONENTS gobject)
FIND_PACKAGE(PkgConfig)

# ask pkg-cfg for gstreamer
PKG_CHECK_MODULES(PC_GSTREAMER QUIET gstreamer-1.0)

# Let CMake find the include files and the library
FIND_PATH(GSTREAMER_INCLUDE_DIR
    NAMES gst/gst.h
    HINTS ${PC_GSTREAMER_INCLUDE_DIRS} ${PC_GSTREAMER_INCLUDEDIR}
    PATH_SUFFIXES gstreamer-1.0
)
mark_as_advanced(GSTREAMER_INCLUDE_DIR)

FIND_LIBRARY(GSTREAMER_LIBRARY
    NAMES gstreamer-1.0
    HINTS ${PC_GSTREAMER_LIBRARY_DIRS} ${PC_GSTREAMER_LIBDIR}
)
mark_as_advanced(GSTREAMER_LIBRARY)

# Process standard args
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GStreamer DEFAULT_MSG GSTREAMER_INCLUDE_DIR GLIB_INCLUDE_DIRS GLIB_LIBRARIES
                                  GLIB_GOBJECT_LIBRARIES GSTREAMER_LIBRARY)

# Add GLib, GObject and threads to GStreamer
set(GSTREAMER_INCLUDE_DIRS ${GLIB_INCLUDE_DIRS} ${GSTREAMER_INCLUDE_DIR})
set(GSTREAMER_LIBRARIES ${GLIB_LIBRARIES} ${GLIB_GOBJECT_LIBRARIES} ${GSTREAMER_LIBRARY} Threads::Threads)