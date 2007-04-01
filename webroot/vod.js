var url = document.location.href;
var backurl = GetBackUrl();

function GetBackUrl()
{
	var i = url.lastIndexOf("back=");
	return (i > 0) ? url.substr(i + 5) : vod_start_page;
}

function SetValue(id, val)
{
	document.getElementById(id).value = val;
}

function PlayNext()
{
	var mpd = document.getElementById("mpdframe");
	mpd.src = mpd_url + "?action=play&arg=" + mpd_args;
}

function Remove(index)
{
	var mpd = document.getElementById("mpdframe");
	mpd.src = mpd_url + "/playlist?action=del&arg=" + index;
}

function Pin(index)
{
	var mpd = document.getElementById("mpdframe");
	mpd.src = mpd_url + "/playlist?action=pin&arg=" + index;
}

function Command(cmd)
{
	var mpd = document.getElementById("mpdframe");
	mpd.src = mpd_url + "?action=command&arg=" + cmd;
}

function SwitchChannel()
{
	Command('switch_audio')	
}

function Add(obj)
{
	var frm = document.getElementById("mpfrm");
	var i = obj.firstChild.nodeValue.indexOf(' ');
	SetValue("mptitle", obj.firstChild.nodeValue.substr(i + 1));
	SetValue("mpstream", vodhost + "/vodstream?id=" + obj.name);
	SetValue("mpaction", "add");
	SetValue("mparg", "");
	frm.action = mpd_url + "/playlist";
	frm.submit();
	obj.className  = "selected";
}

function MakeFloat(id)
{
	document.getElementById(id).style.top = window.pageYOffset + 'px';
	//setTimeout("MakeFloat('" + id + "')", 1000);
}

function Go(newurl, backurl)
{
	if (backurl)
		newurl += (newurl.indexOf("?") > 0 ? "&" : "?") + "back=" + backurl;
	document.location.href = newurl;
}

function PageDown()
{
	var from = GetUrlArg("from");
	if (!from) {
		Go(url + "&from=" + item_per_page);
		return 
	}
	var fromint = parseInt(from) + item_per_page;
	var i = url.indexOf("from=")
	var j = url.indexOf("&", i);
	var newurl = url.substr(0, i + 5)  + fromint;
	if (j > 0) newurl += url.substr(j);
	Go(newurl, backurl);
}

function PageUp()
{
	var from = GetUrlArg("from");
	if (!from) return;
	var fromint = parseInt(from) - item_per_page;
	if (fromint < 0) fromint = 0;
	var i = url.indexOf("from=")
	var j = url.indexOf("&", i);
	var newurl = url.substr(0, i + 5)  + fromint;
	if (j > 0) newurl += url.substr(j);
	Go(newurl, backurl);
}

function GetUrlArg(name)
{
	var idx=document.location.href.indexOf(name+'=');
	if (idx<=0) return null;
	var argstr=document.location.href.substring(idx+name.length+1);
	idx = argstr.indexOf('&');
	return idx>=0?argstr.substring(0, idx):argstr;
}

function DefKeyEvents(e)
{
	if (window.onKeyPress && onKeyPress(e.which)) return;
	if (e.which >= 48 && e.which <= 57) {
		if (window.onNumKeys) onNumKeys(e.which - 48);
		return;
	}
	switch (e.which) {
	case key_back:
		Go(backurl, null);
		break;
	case key_page_down:
		PageDown();
		break;
	case key_page_up:
		PageUp();
		break;
	case key_play:
		PlayNext();
		break;
	case key_channel:
		SwitchChannel();
		break;
	}
}

document.onkeypress = DefKeyEvents;