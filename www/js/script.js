// JavaScript Document


$(document).ready(function(){
	$('div.rounded').wrap('<div class="cadre"></div>');
	$('div.cadre').prepend('<div class="cadre_hd"></div><div class="cadre_hg"></div>');
	$('div.cadre').append('<div class="cadre_bd"></div><div class="cadre_bg"></div>');
});


$(document).ready(function() {
	$('#slider').s3Slider({
		timeOut: 4000
	});
});
