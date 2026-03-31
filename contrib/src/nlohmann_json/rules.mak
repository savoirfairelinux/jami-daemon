# NLOHMANN_JSON
# Header-only JSON library required by opentelemetry-cpp's OTLP HTTP exporter.
# Version must match what opentelemetry-cpp v1.19.0 expects (see third_party_release).
NLOHMANN_JSON_VERSION := 3.11.3
NLOHMANN_JSON_URL := https://github.com/nlohmann/json/archive/v$(NLOHMANN_JSON_VERSION).tar.gz

NLOHMANN_JSON_CONF = -DJSON_BuildTests=OFF \
	-DJSON_Install=ON \
	-DJSON_MultipleHeaders=OFF

$(TARBALLS)/nlohmann_json-$(NLOHMANN_JSON_VERSION).tar.gz:
	$(call download,$(NLOHMANN_JSON_URL))

.sum-nlohmann_json: nlohmann_json-$(NLOHMANN_JSON_VERSION).tar.gz

nlohmann_json: nlohmann_json-$(NLOHMANN_JSON_VERSION).tar.gz .sum-nlohmann_json
	$(UNPACK)
	mv json-$(NLOHMANN_JSON_VERSION) $(UNPACK_DIR)
	$(MOVE)

CMAKE_PKGS += nlohmann_json
