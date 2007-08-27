<?php

/**
 * Copyright (C) 2007 - Savoir-Faire Linux Inc.
 * Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *
 * LICENCE: GPL
 */


require_once('config.inc.php');


/**
 * Helper functions to translate AsciiDoc files into HTML, display
 * them and cache them.
 */


/**
 * Show page, compile it if new, cache it.
 *
 * @return HTML content
 */
function show_page($page) {
  // Compile it
  
  // Get the latest HASH for that page.
  $hash = get_git_hash($page);

  $cnt = get_cache_hash($hash);

  if (!$cnt) {
    $cnt = compile_page($hash, $page /* for ref only */);

    put_cache_hash($hash, $cnt);

    return $cnt;
  }

  return $cnt;
  
}


/**
 * Create the Cache dir if it doesn't exist.
 */
function check_cache_path() {
  global $CACHE_PATH;

  if (!file_exists($CACHE_PATH)) {
    mkdir($CACHE_PATH);
  }
}


/**
 * Check if the cache dir if this object was cached, if so, return it.
 *
 * @return mixed NULL or the content of the cache
 */
function get_cache_hash($hash) {
  global $CACHE_PATH;
  
  $fn = $CACHE_PATH.'/'.$hash.'.cache';

  if (file_exists($fn)) {
    return file_get_contents($fn);
  }

  return NULL;
}


/**
 * Write content to cache (identified by $hash)
 */
function put_cache_hash($hash, $content) {
  global $CACHE_PÂTH;
  
  $fn = $CACHE_PATH.'/'.$hash.'.cache';

  file_put_contents($fn, $content);

  return true;
}


/**
 * Just compile the page
 *
 * @return string Content of the compiled page.
 */
function compile_page($hash, $page) {
  global $GIT_REPOS;

  $output = '';

  $p = popen("GIT_DIR=".$GIT_REPOS." git-show $hash | asciidoc -", 'r');

  if (!$p) {
    return "Unable to compile file: $page ($hash)\n";
  }

  while (!feof($p)) {
    $output .= fread($p);
  }
  pclose($p);

  return $output;
}


/**
 * Retrieve file from git's object-ocean
 *
 * UNUSED
 */
function get_git_file_content($file) {
  $hash = get_git_hash($file);

  $content = get_git_hash_content($hash);

  return $content;
}


/**
 * Retrieve hash's content from git's object-ocean
 *
 * UNUSED
 */
function get_git_hash_content($hash) {
  global $GIT_REPOS;

  $content = exec("GIT_DIR=".$GIT_REPOS." git-show $hash");

  return $content;
}

/**
 * Get the file's SHA-1 hash, for the latest revision on USE_BRANCH
 *
 * Used for comparison of cached/to cache/cache filename.
 *
 * @return string SHA-1 hash
 */
function get_git_hash($file) {
  global $USE_BRANCH, $GIT_REPOS;

  $output = array();

  $string = exec("GIT_DIR=".$GIT_REPOS." git-ls-tree $USE_BRANCH \"".git_filename($file)."\"", $output);

  if (count($output)) {
    $fields = explode(' ', $output[0]);

    if ($fields[1] == 'blob') {
      // Return the HASH
      return $fields[2];
    }
  }

  return NULL;
}

/**
 * Get file name (parsed and clear for git-ls-tree)
 *
 * @return string Parsed file name
 */
function git_filename($file) {
  global $PREFIX;
  // remove all '//', for '/', remove the trailing '/' at the beginning
  // add the PREFIX

  $out = $PREFIX . '/' . $file;

  $out = str_replace('//', '/', $out);
  $out = str_replace('//', '/', $out); // In case there are 3 in a row

  $out = ltrim($out, '/');

  return $out;
}




/** Check cache path right away, at each run. */

check_cache_path();


?>