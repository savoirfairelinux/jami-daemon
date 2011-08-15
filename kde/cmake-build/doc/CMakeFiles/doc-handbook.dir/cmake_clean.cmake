FILE(REMOVE_RECURSE
  "CMakeFiles/doc-handbook"
  "index.cache.bz2"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/doc-handbook.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
