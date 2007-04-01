var window_width = 240;
var window_height = 240;
var href = document.location.href;
var vodhost = href.substr(0, href.indexOf('/', 8));
var vodport = 81;
var mpd_url = "http://localhost/mpd";
var mpd_args = "";
var item_per_page = 10;

var vod_start_page = "/vod.html";
var vod_cats_url = "vodcats.html";
var vod_chars_url = "vodchars.html";
var playlist_url = "vodplaylist.html";