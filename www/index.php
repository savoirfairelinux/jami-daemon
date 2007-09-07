<?php
/**
 * Copyright (C) 2007 - Savoir-faire Linux Inc.
 * Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *
 * LICENSE: GPLv3
 */

require_once('sflphone.funcs.php');


// We've requested an image, fetch it, and send it.
if (defined($_REQUEST['i'])) {
  $img = $_REQUEST['i'];
  switch(strtolower(substr($img, -3))) {
  case 'jpg':
    header("Content-Type: image/jpeg");
    break;
  case 'png':
    header("Content-Type: image/png");
    break;
  case 'gif':
    header("Content-Type: image/gif");
    break;
  default:
    break;
  }

  show_page($img);
}



$module = '';
// Default module: home
if (!$_REQUEST['mod']) {
  $module = 'home';
} else {
  $module = $_REQUEST['mod'];
}




// Send output.
include('header.php');

$mod = "templates/page_$module.php";
if (!file_exists($mod)) {
  print "<h1>Module '$module' not found</h1>";
} else {
  include($mod);
}

include('footer.php');

?>