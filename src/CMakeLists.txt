project(lib)

file(GLOB SRCS "*.c")

file(GLOB HEADERS "peer.h" "peer_connection.h" "peer_signaling.h")

add_library(peer
  ${SRCS}
  ${HTTP_SOURCES}
  ${MQTT_SOURCES}
  ${MQTT_SERIALIZER_SOURCES}
)

target_include_directories(peer PRIVATE
  ${HTTP_INCLUDE_PUBLIC_DIRS}
  ${MQTT_INCLUDE_PUBLIC_DIRS}
  $ENV{ARMINO_PATH}/components/psa_mbedtls/mbedtls/include
	$ENV{ARMINO_PATH}/components/psa_mbedtls/mbedtls/library
	$ENV{ARMINO_PATH}/components/psa_mbedtls/mbedtls_port/inc
	$ENV{ARMINO_PATH}/components/psa_mbedtls/mbedtls/include/mbedtls
	$ENV{ARMINO_PATH}/components/psa_mbedtls/mbedtls_port/configs
  ${CMAKE_CURRENT_LIST_DIR}/include/
)

target_link_libraries(peer PRIVATE "srtp2" "armino::psa_mbedtls" "armino::mjson")

set_target_properties(peer PROPERTIES PUBLIC_HEADER "${HEADERS}")

