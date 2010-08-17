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
	var msgBody = document.getElementById ('messages');
	msgBody.innerHTML = msgBody.innerHTML + '<div class="message"><div class="peername">' + peer_name + '</div><div class="peernumber">' +  peer_number  + '</div>' + linkify (message) + '</div>' ;
}

