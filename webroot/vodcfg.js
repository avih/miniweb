var window_width = 240;
var window_height = 180;
var href = document.location.href;
var vodhost = href.substr(0, href.indexOf('/', 8));
var mpd_url = "http://localhost/mpd";
var mpd_args = "";
var item_per_page = 10;
var key_pin = 47;		// key /
var key_remove = 46;	// key .
var key_play = 13;
var key_back = 0;
var key_page_down = 43;	// key +
var key_page_up = 45;	// key -
var key_channel = 42;	// key *

var vod_start_page = "/vod.html";
var vod_cats_url = "/vodlib/category?xsl=/vodcats.xsl&count=10";
var vod_chars_url = "/vodlib/chars?xsl=/vodchars.xsl&count=10";