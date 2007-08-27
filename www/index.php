<?php
/**
 * Copyright (C) 2007 - Savoir-faire Linux Inc.
 * Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *
 * LICENSE: GPLv3
 */

require_once('sflphone.funcs.php');



$module = '';

// Default module: home
if (!$_REQUEST['l']) {
  $module = 'home';
} else {
  $module = $_REQUEST['l'];
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