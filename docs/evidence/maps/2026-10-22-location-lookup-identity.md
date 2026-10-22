# Location lookup identity and validation — October 22, 2026

## Reproduced defects and narrow correction

The actual lookup worker stored one current result but kept the last successful
address as a separate cache key. Address A → IP area → address A could return
the IP result as a cached address result. The new production-code test reproduces
this at `location_lookup_tests.cpp:51`. The correction acquires existing request
ownership before cache access and compares the requested address with `g_query`,
which identifies the currently published result. IP requests replace that query
with the empty string. Busy requests remain rejected and immediate repeats of
the current successful address remain cached. The obsolete last-address buffer
is removed; no public API or persistence format changes.

The worker also parsed address coordinate strings using `strtod` without checking
where parsing ended. Empty/nonnumeric strings could become zero and numeric
prefixes could be accepted. After the cache correction, the second regression
reproduces invalid data becoming ready at line 55. A small shared coordinate
parser now requires a nonempty complete numeric conversion; existing finite and
latitude/longitude range checks still apply. It does not write receiver location.

Changed production source: `apps/orcsdr-tab5/ui/location_estimate.cpp`. Tests:
`apps/orcsdr-tab5/tests/maps/location_lookup/` and two additional declarations
in the existing fake HTTP SDK header. The tests compile the real worker and
the SDK's actual cJSON source; only HTTP responses and task scheduling are faked.
No network service, Wi-Fi credentials, SD or Tab5 is accessed by these tests.

## Exact focused commands and results

From the integration worktree root, using WSL:

```text
cmake -S apps/orcsdr-tab5/tests/maps/location_lookup -B .local/maps/location-lookup-host-20261022 -DCJSON_ROOT=/mnt/c/Espressif/frameworks/esp-idf-v5.5.4/components/json/cJSON
cmake --build .local/maps/location-lookup-host-20261022 -j2
ctest --test-dir .local/maps/location-lookup-host-20261022 --output-on-failure
cmake -S apps/orcsdr-tab5/tests/maps/location_lookup -B .local/maps/location-lookup-asan-20261022 -DCJSON_ROOT=/mnt/c/Espressif/frameworks/esp-idf-v5.5.4/components/json/cJSON -DCMAKE_C_FLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_CXX_FLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build .local/maps/location-lookup-asan-20261022 -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir .local/maps/location-lookup-asan-20261022 --output-on-failure
cmake --build .local/maps/labels-host --target orcsdr_map_http_stream_tests -j2
ctest --test-dir .local/maps/labels-host -R '^map_http_stream$' --output-on-failure
```

Normal lookup **1/1 PASS**, sanitized lookup **1/1 PASS**, shared HTTP adapter
**1/1 PASS**. Cases include cache method/query identity, same-address cache hit,
busy rejection, empty/nonnumeric/partially numeric/nonfinite/out-of-range
coordinate rejection, HTTP 503 and task-creation failure/retry. Production task
stack/core/caps and HTTPS bundle/User-Agent configuration are checked at the
fake boundary; real scheduling/TLS/memory are not exercised.

P4 syntax command:

```text
python .local/maps/check_location_lookup_syntax_20261022.py
```

**PASS**, using the native compile database. Logs in the worktree root:
`location-lookup-red-{configure,build,test}-20261022.log`,
`location-lookup-coordinate-red-{build,test}-20261022.log`,
`location-lookup-host-final-{build,test}-20261022.log`,
`location-lookup-asan-{configure,build,test}-20261022.log`,
`location-lookup-asan-final-{build,test}-20261022.log`,
`location-lookup-http-adapter-{build,test}-20261022.log` and
`location-lookup-p4-syntax-20261022.log`.

## Separate production geocoding gate

Source inspection also confirms the current address service URL is hard-coded
to public Nominatim. Its official policy, checked October 22, states an aggregate
application limit of one request per second and requires apps to be able to
switch services without a software update. It also specifies identifying
User-Agent, attribution, caching and no autocomplete/systematic querying.
[Official policy](https://operations.osmfoundation.org/policies/nominatim/).

That public endpoint must not be treated as the production solution for the
20,000-user scenario without resolving those constraints. No live address query
was sent in this investigation. The deterministic fixtures above exercise the
worker's address/IP sequence without calling any provider. Production provider
or owned proxy selection and runtime service-switch configuration remain open;
they are separate from the map-archive GitHub pilot approval. Do not substitute
a generic hard-coded service or promise that offline packs include address search.

Native linking, device boot/IP lookup/failure handling, live address service
acceptance and complete self-contained setup remain unproven by these host checks.
