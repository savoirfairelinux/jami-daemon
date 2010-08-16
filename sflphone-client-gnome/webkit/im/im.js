function add_message (message)
{
	var msgBody = document.getElementById ('messages');
	msgBody.innerHTML = '<div class="message">' + message + '</div>' + msgBody.innerHTML;
}

