var url = document.location.href;
var xmlhttp = new XMLHttpRequest;
var backurl = GetBackUrl();

function GetBackUrl()
{
	var i = url.indexOf("back=");
	return (i > 0) ? url.substr(i + 5) : vod_start_page;
}

function loadXML(xmlFile)
{
	var $xml = new XMLHttpRequest;
	$xml.open('GET', xmlFile, false);
	$xml.overrideMimeType('text/xml');
	$xml.send(null);
	var xml = $xml.responseXML;
	if (!xml) {
		alert("Unable to load "+xmlFile);
		return null;
	}
	return xml;
}

function transformXML(xmlDoc, xslDoc, element)
{
	var XSLT = new XSLTProcessor;
	XSLT.importStylesheet(xslDoc);
	var e = document.getElementById(element);
	e.innerHTML = "";
	if (e) e.appendChild(XSLT.transformToFragment(xmlDoc, document));
}

function SetValue(id, val)
{
	document.getElementById(id).value = val;
}

function PlayNext()
{
	var requrl = "/vodplay?action=control&arg=1";
	xmlhttp.open("GET", requrl, false);
	xmlhttp.send(null);
}

function Remove(index)
{
	var requrl = "/vodplay?action=del&arg=" + index;
	xmlhttp.open("GET", requrl, false);
	xmlhttp.send(null);
	var result = xmlhttp.responseXML.getElementsByTagName("state");
	return (result.length > 0 && result[0].childNodes[0].nodeValue == "OK");
}

function Pin(index)
{
	var requrl = "/vodplay?action=pin&arg=" + index;
	xmlhttp.open("GET", requrl, false);
	xmlhttp.send(null);
	var result = xmlhttp.responseXML.getElementsByTagName("state");
	return (result.length > 0 && result[0].childNodes[0].nodeValue == "OK");
}

function Command(cmd)
{
	/*
	var mpd = document.getElementById("mpdframe");
	mpd.src = mpd_url + "?action=command&arg=" + cmd;
	*/
}

function SwitchChannel()
{
	var requrl = "/vodplay?action=control&arg=2";
	xmlhttp.open("GET", requrl, false);
	xmlhttp.send(null);
}

function Add(id, title)
{
	var requrl = "/vodplay?action=add&stream=" + vodhost + ":" + vodport + "/vodstream?id=" + id + "&title=" + title;
	xmlhttp.open("GET", requrl, false);
	xmlhttp.send(null);
	var result = xmlhttp.responseXML.getElementsByTagName("state");
	return (result.length > 0 && result[0].childNodes[0].nodeValue == "OK");
}

function Go(newurl)
{
	document.location.href = newurl;
}

/*
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
*/

function GetUrlArg(name)
{
	var idx=document.location.href.indexOf(name+'=');
	if (idx<=0) return null;
	var idxback = document.location.href.indexOf('back=');
	if (idxback >0 && idx > idxback) return null;
	var argstr=document.location.href.substring(idx+name.length+1);
	idx = argstr.indexOf('&');
	return idx>=0?argstr.substring(0, idx):argstr;
}

function DefKeyEvents(e)
{
	var KeyID = (window.event) ? event.keyCode : e.keyCode;
	if (window.onKeyPress && onKeyPress(KeyID)) return;
	switch (KeyID) {
	case 27:	// ESC
	case 74:	// j
		history.go(-1);
		break;
	case 70:
	case 109:	// -
	case 73:	// i
		PageUp();
		break;
	case 71:
	case 107:	// +
	case 61:	// =
	case 75:	// k
		PageDown();
		break;
	case 13:	// enter
	case 32:	// space
	case 78:	// n
		PlayNext();
		break;
	case 16:	// shift
	case 106:	// *
	case 9:
	case 67:	// c
		SwitchChannel();
		break;
	case 17:	// ctrl
	case 111:	// /
	case 46:
	case 76:	// l
		Go(playlist_url);
		break;
	case 77:	// m
	case 220:	// \
	case 192:	// `
		Go(vod_start_page);
		break;
	case 35:	// keypad 1
		KeyID = 49;
		break;
	case 40:	// keypad 2
		KeyID = 50;
		break;
	case 34:	// keypad 3
		KeyID = 51;
		break;
	case 37:	// keypad 4
		KeyID = 52;
		break;
	case 12:	// keypad 5
	case 0:		// keypad 5 on linux
		KeyID = 53;
		break;
	case 39:	// keypad 6
		KeyID = 54;
		break;
	case 36:	// keypad 7
		KeyID = 55;
		break;
	case 38:	// keypad 8
		KeyID = 56;
		break;
	case 33:	// keypad 9
		KeyID = 57;
		break;
	case 45:	// keypad 0
		KeyID = 48;
		break;
	}
	if (KeyID >= 48 && KeyID <= 57) {
		if (window.onNumKeys) onNumKeys(KeyID - 48);
	}
}

document.onkeyup = DefKeyEvents;