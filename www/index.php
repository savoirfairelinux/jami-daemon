<?php
$name = 'home';
$javascript = array('jquery','s3Slider','script','animated-menu');
include('include/header.php');

?>

<div id="content">
    <table class="pourcentWidth" cellpadding="0" cellspacing="0">
        <tbody valign="top">
            <tr>
                <td id="index_td_gauche">
                    <table class="tabsIndex" cellpadding="0" cellspacing="0">
                        <tbody valign="top">
                            <tr>
                                <td class="imgSliderTd">
								    <!-- // slider -->
									<div id="slider" >
                                        <ul id="sliderContent">
                                            <li class="sliderImage">
                                                <img src="img/slider/lady-wise.jpg"      width="310" height="384" alt=""/>
                                                <span class="bottom">
                                                <strong>Savoir-Faire Linux</strong>
                                                <br />presents SFLphone</span>
                                            </li>
                                            <li class="sliderImage">
                                                <img src="img/slider/mrbad.jpg" width="310" height="384" alt=""/>
                                                <span class="bottom">
                                                <strong>SFLphone</strong>
                                                <br />It's free and you can use it everywhere.</span>
                                            </li>
                                            <li class="sliderImage">
                                                <img src="img/slider/homepage.png" width="310" height="384" alt=""/>
                                                <span class="bottom">
                                                <strong>SFLphone</strong>
                                                <br />SIP and IAX2 softphone</span>
                                            </li>
                                            <li>
                                                <div class="clear sliderImage">&nbsp;</div>
                                            </li>
                                        </ul>
									</div>
                                </td>
                                <td>
                                <br> <br> <br>                                        
                                    <ul class="fntBleu list" style="padding:10px;">
                                        <li>SIP and IAX2 compatible softphone</li>
                                        <li>Robust enterprise-class desktop phone for Linux</li>
                                        <li>Provide support of unlimited number of calls, multi-accounts, call transfer and hold.</li>
                                        <li>Enjoy a perfect audio quality</li>
                                    </ul> 
                                    <br>
                                    <div class="float_it_middle">
                                        <a href="download.php" >
                                        <img src="img/download_icon.png" width="61" height="68" alt=""/>
                                        </a>
                                    </div> 
                                    <div class="float_it_middle">
                                        <p class="highlight"> 
                                        <a href="download.php" >
                                        Download now!</a>
                                        </p>
                                    </div>
                                </td>
                            </tr>
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

include ('include/footer.php');
