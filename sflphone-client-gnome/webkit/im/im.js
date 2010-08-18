function linkify(text){
    if (text) {
        text = text.replace(
            /((https?\:\/\/)|(www\.))(\S+)(\w{2,4})(:[0-9]+)?(\/|\/([\w#!:.?+=&%@!\-\/]))?/gi,
            function(url){
                var full_url = url;
                if (!full_url.match('^https?:\/\/')) {
                    full_url = 'http://' + full_url;
                }
                return '<a href="' + full_url + '">' + url + '</a>';
            }
        );
    }
    return text;
}


function add_message (message, peer_name, peer_number, peer_info)
{
	var display_name = 'Unknown';
	if (peer_name != '') {
		display_name = peer_name;
	}
	
	var msgBody = document.getElementById ('messages');
	msgBody.innerHTML = msgBody.innerHTML + '<div class="message">' +  '<span class="peername">' + display_name + ': </span>' + '<span class="text">' + linkify (message) + '</span></div>' ;
}

function add_call_info_header (peer_name, peer_number, peer_info)
{
	var infoBody = document.getElementById ('call-info');
	infoBody.innerHTML = '<h2>Calling ' + peer_number + '</h2><p>' + peer_info + '</p></h4>';

}
