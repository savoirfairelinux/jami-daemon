<?php
$name = 'home';
$javascript = array(
                'jquery',/*'s3Slider','script',*/
                'animated-menu'
            );

include_once('include/header.php');
?>

<div id="content">
    <table class="pourcentWidth" cellpadding="0" cellspacing="0">
        <tbody valign="top">
            <tr>
                <td id="index_td_gauche">
                    <table class="tabsIndex" cellpadding="0" cellspacing="0">
                        <tbody valign="top">
                            <tr>
                                <td>
                                    <ul class="thumbnails">
                                        <li><a href="/screenshot.php#img/gallery/main-view.png"><img class="thumb" src="img/gallery/thumbs/main-view.png" /></a></li>
                                        <li><a href="/screenshot.php#img/gallery/main-history-opt.png"><img class="thumb" src="img/gallery/thumbs/main-history-opt.png" /></a></li>
                                        <li><a href="/screenshot.php#img/gallery/main-transer.png"><img class="thumb" src="img/gallery/thumbs/main-transfer.png" /></a></li>
                                        <!--<li><a href="/screenshot.php#img/gallery/incoming-call.png"><img class="thumb" src="img/gallery/thumbs/incoming-call.png" /></a></li>-->
                                    </ul>
                                </td>
                                <td>
                                    <ul class="fntBleu list" style="padding:10px;">
                                        <li>SIP and IAX2 compatible softphone</li>
                                        <li>Robust enterprise-class desktop phone for Linux</li>
                                        <li>Provide support of unlimited number of calls, multi-accounts, call transfer and hold.</li>
                                        <li>Enjoy perfect audio quality</li>
                                        <li>Gnome integration</li>
                                    </ul> 
                                    <div class="float_it_middle">
                                        <a href="download.php" >
                                            <div class="download">
                                                <span class="free-download">Free download</span>
                                                <span class="sfl-version">Linux version: 0.9.3-3</span>
                                            </div>
                                        </a>
                                    </div>
                                    <div class="separator">&nbsp;</div>
                                    <div class="float_it_middle">
                                    <form action="https://www.paypal.com/cgi-bin/webscr" method="post">
                                        <input type="hidden" name="cmd" value="_s-xclick">
                                        <input type="hidden" name="hosted_button_id" value="3918066">
                                        <input type="image" 
                                        src="https://www.paypal.com/en_US/i/btn/btn_donateCC_LG.gif" border="0" 
                                        name="submit" alt="PayPal - The safer, easier way to pay online!">
                                        <img alt="" border="0" 
                                        src="https://www.paypal.com/fr_XC/i/scr/pixel.gif" width="1" height="1">
                                    </form>
                                    </div>
                                </td>
                            </tr>
                            <!--<tr>
                                <td>
                                </td>
                            </tr>-->
                        </tbody>
                    </table>  
                </td>
                <td id="index_td_separator">&nbsp;</td>
                <td id="index_td_droite">
                    <p class="title alignCenter fntBleu">
                    Discover SFLphone	
                    </p>
                    <p>
                    SFLphone is a SIP/IAX2 compatible softphone for Linux. 
                    The SFLphone project's goal is to create a robust enterprise-class desktop phone. 
                    While it can serve home users very well, it is designed with a hundred-calls-a-day receptionist in mind.
                    <br/><br/>
                    SFLphone is free software and is distributed under the <b>GNU General Public License version 3</b>. 
                    It is developed by <a href="http://www.savoirfairelinux.com">Savoir-Faire Linux</a>, 
                    a Canadian Linux consulting company, in partnership with the global community. 
                    Savoir-Faire Linux provides to users easy-to-install packages for most of the main Linux distributions.  
                    </p>
                    <img class="discover-image" src="img/slider/lady-discover.jpg" />
                </td>
            </tr>
        </tbody>
    </table>
</div>
<?php
include_once ('include/footer.php');
?>
