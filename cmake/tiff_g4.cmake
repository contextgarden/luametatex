set(tiff_g4_sources

  source/libraries/tiff_g4/tiff_g4.c

)

add_library(tiff_g4 STATIC ${tiff_g4_sources})

target_include_directories(tiff_g4 PRIVATE
    source/libraries/tiff_g4
)
