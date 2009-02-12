<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">

<head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <title>SFLphone - Official Website</title>
    <link rel="stylesheet" type="text/css" id="style1" href="css/style1.css" />
    <link rel="stylesheet" type="text/css"  href="css/s3Slider.css" media="screen"/>
    <link rel="shortcut icon" href="favicon.ico" /> 
    <!--  jquery  -->
  <?php
    //Inlucde javascript
    $javascript = array_merge(array(),(array)$javascript);
    foreach($javascript as $script){
      echo "<script type='text/javascript' src='js/$script.js'></script>";
    }
  //<script type="text/javascript" src="js/jquery.js"></script>
	//<script type="text/javascript" src="js/s3Slider.js"></script>
	//<script type="text/javascript" src="js/script.js"> </script>
	//<script type="text/javascript" src="js/animated-menu.js"></script>
  ?>
</head>

<body>
    <div id="header">
        <!--//Background image -->
    </div>
 
    <div id="global">

        <div id="controller">
        <!-- Taille fixe ou Dynamique -->

        <!-- Nav -->
        <table class="navTabs" cellpadding="0" cellspacing="0">
            <tbody valign="top">
                <tr>
                    <td  class="option">
                    <div id="option">
                    </div>
                </td>

                <td  class="navigation">
                    <div id="navigation">
                        <div class="dynamic-nav" style="background: #CDCDCD;" >&nbsp;</div>
                            <ul>
                                <li <?php echo ($name == 'contact')?'class="active"':"" ?>><a href='contact.php'><span>Contact</span></a></li>
                                <li <?php echo ($name == 'wiki')?'class="active"':"" ?>><a href='http://dev.savoirfairelinux.net/sflphone/'><span>Wiki</span></a></li>
                                <li <?php echo ($name == 'download')?'class="active"':"" ?>><a href='download.php'><span>Download</span></a></li>
                                <li <?php echo ($name == 'feature')?'class="active"':"" ?>><a href='features.php'><span>Features</span></a></li>
                                <li <?php echo ($name == 'home')?'class="active"':"" ?>><a href='index.php'><span>Home</span></a></li>
                            </ul>
                        </div>
                </td>
            </tr>
        </tbody>
    </table>
