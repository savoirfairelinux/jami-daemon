
(function($){

$.fn.animatedMenu = function(){

  var element = this;

  
      $(element).before("<div class='dynamic-nav' style='background: #CDCDCD;' >&nbsp;</div>");
      
      var activePos;
      $(element).children('li.active:first').each(
              function(){
                activePos = { left: this.offsetLeft, top: this.offsetTop, width: this.offsetWidth, height: this.offsetHeight}
              }
            );
      $('div.dynamic-nav').hide(0,function(){$(this).animate(activePos,0);});
      
      $(element).children('li').hover(
        function(){
          var liPos = { 
              left: this.offsetLeft, 
              top: this.offsetTop, 
              width: this.offsetWidth, 
              height: this.offsetHeight
          }
          
          $('div.dynamic-nav').show().animate(liPos,200);
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
