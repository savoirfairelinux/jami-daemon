
(function($){

$.fn.animatedMenu = function(){

  var element = this;

  
      $(element).before("<div class='dynamic-nav' style='background: #CDCDCD;' >&nbsp;</div>");
      
      var activePos;
      $(element).children('li.active:first').each(
              function(){
                this.opacity = 0;
                activePos = { left: this.offsetLeft, top: this.offsetTop, width: this.offsetWidth, height: this.offsetHeight, opacity: 0}
              }
            );
      $('div.dynamic-nav').hide(1,function(){$(this).animate(activePos,1);});
      
      $(element).children('li').hover(
        function(){
          var liPos = { 
              left: this.offsetLeft, 
              top: this.offsetTop, 
              width: this.offsetWidth, 
              height: this.offsetHeight,
              opacity: 1
          }
     
          $('div.dynamic-nav').stop().show().animate(liPos,500);
        },
        function(){
          $('div.dynamic-nav').hide();
        }
      );
      
      $(element).children('li.active').hover(
        function(){
          $(this).removeClass('active');
        },
        function(){
          $(this).addClass('active');
        }
      );
};

})(jQuery);

$(document).ready(function(){
  $('#navigation > ul').animatedMenu();
  }
);
