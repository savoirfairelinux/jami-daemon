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


function add_message (message, peer_name, class_additionnal, time)
{
	var msgBody = document.getElementById ('messages');
	msgBody.innerHTML = msgBody.innerHTML + '<div class="message ' + class_additionnal + '">' +  '<span class="author">[' + peer_name + '] </span><span class="message-time">' + time + '</span><p class="text">' + linkify (message) + '</p></div>' ;
	document.getElementById("bottom").scrollIntoView(true);
}

function add_call_info_header (peer_name, peer_number)
{
	var peerNumber = document.getElementById ('peer-number');
	var peerName = document.getElementById ('peer-name');
	var peerInfo = document.getElementById ('peer-info');
	peerNumber.innerHTML = peer_number;
	peerName.innerHTML = peer_name;

}

function open_url (uri) {
	window.open(''+self.location,'mywin',
	'left=20,top=20,width=500,height=500,toolbar=1,resizable=0');

}
