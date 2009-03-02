<?php
$name = 'screenshot';
$javascript = array('jquery','animated-menu','jquery.galleria');
$css = array('galleria');

include_once('utils.php');

include('include/header.php');

?>
    <div id="content">
      <div id="main-image">&nbsp;</div>
      <ul class="gallery">
        <?php
        //  $imageDirectory = "./img/gallery";
        //  $files = scandir($imageDirectory);
          
        //  array_shift($files); // shift folder "."
        //  array_shift($files); // shift folder ".."          
       // //  print_r($files);
       //   foreach($files as $file):
       // 
        //<li><a href="img/gallery/<?php echo $file " title="Main Window"><img src="img/gallery/thumbs/<?php echo $file " alt="Main Window"></a></li> 
        //php endforeach; 
        $images = array(
          array('assistant-1.png','File menu', 'state' => 'active'),
          array('main-edit.png','Edit menu'),
          array('main-view.png','View menu'),
          array('main-help.png','Help menu'),
          array('assistant-2-.png','Installation wizard welcome'),
          array('assistant-3-.png','Account type selector'),
          array('assistant-4-.png','Account configuration'),
          array('assistant-5-.png','Nat configuration'),
          array('edit-account.png','General settings'),
          array('edit-pref-gen.png','General settings'),
          array('edit-pref-audio.png','Audio settings'),
          array('edit-pref-rec.png','Record settings'),
          array('incoming-call.png','Incoming call'),
          array('main-history.png','Call history'),
          array('main-history-opt.png','Call history options'),
          array('main-transfer.png','Call transfer'),
          array('account-edit.png','Account add dialog'),
          array('account-add.png','Account edit dialog')          
        );
        
        
        foreach($images as $image){
          $fileName    = $image[0];
          $description = $image[1];
          echo "<li ";
          if($image['state'] != null)
            echo 'class="active" > ';
          echo "<a href='img/gallery/$fileName' title='$description'><img src='img/gallery/thumbs/$fileName' alt='$description'></a></li>";
        }
        
        ?>
      </ul>
    </div>
    <!--
        Thanks to http://devkick.com/lab/galleria/ for doing the galleria plugin. 
        The code snipet bellow is taken from the demo1. It probably saved many hours of work.
        
        I should thank all the jQuery team and users for making one of the most awesome javascript library out there.
        http://jquery.org
    -->
    <script type="text/javascript">       	
	    jQuery(function($) {
		
		    $('.gallery_demo_unstyled').addClass('gallery_demo'); // adds new class name to maintain degradability
		
		    $('ul.gallery').galleria({
			    history   : true, // activates the history object for bookmarking, back-button etc.
			    clickNext : true, // helper for making the image clickable
			    insert : '#main-image', 
			    onImage   : function(image,caption,thumb) { // let's add some image effects for demonstration purposes
				
				    // fade in the image & caption
				    if(! ($.browser.mozilla && navigator.appVersion.indexOf("Win")!=-1) ) { // FF/Win fades large images terribly slow
					    image.css('display','none').fadeIn(1000);
				    }
				    caption.css('display','none').fadeIn(1000);
				
				    // fetch the thumbnail container
				    var _li = thumb.parents('li');
				
				    // fade out inactive thumbnail
				    _li.siblings().children('img.selected').fadeTo(500,0.3);
				
				    // fade in active thumbnail
				    thumb.fadeTo('fast',1).addClass('selected');
				
				    // add a title for the clickable image
				    image.attr('title','Next image >>');
			    },
			    onThumb : function(thumb) { // thumbnail effects goes here
				
				    // fetch the thumbnail container
				    var _li = thumb.parents('li');
				
				    // if thumbnail is active, fade all the way.
				    var _fadeTo = _li.is('.active') ? '1' : '0.3';
				
				    // fade in the thumbnail when finnished loading
				    thumb.css({display:'none',opacity:_fadeTo}).fadeIn(1500);
				
				    // hover effects
				    thumb.hover(
					    function() { thumb.fadeTo('fast',1); },
					    function() { _li.not('.active').children('img').fadeTo('fast',0.3); } // don't fade out if the parent is active
				    )
			    }
		    });

	    });
      //$('ul.gallery > li:first > img').click();
    </script>
<?php

include('include/footer.php');

?>
