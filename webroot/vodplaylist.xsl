<?xml version="1.0" encoding="utf-8"?><!DOCTYPE xsl:stylesheet  [
	<!ENTITY nbsp   "&#160;">
	<!ENTITY copy   "&#169;">
	<!ENTITY reg    "&#174;">
	<!ENTITY trade  "&#8482;">
	<!ENTITY mdash  "&#8212;">
	<!ENTITY ldquo  "&#8220;">
	<!ENTITY rdquo  "&#8221;"> 
	<!ENTITY pound  "&#163;">
	<!ENTITY yen    "&#165;">
	<!ENTITY euro   "&#8364;">
]>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" encoding="gb2312" doctype-public="-//W3C//DTD XHTML 1.0 Transitional//EN" doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"/>
<xsl:template match="/response/playlist">

<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=gb2312"/>
<title>VOD</title>
<link href="/vod.css" rel="stylesheet" type="text/css"/>
<script language="javascript" src="/vodcfg.js"></script>
<script language="javascript" src="/vod.js"></script>
<script language="javascript">
var sel = new Array(item_per_page);
for (i in sel) sel[i] = 0;

function onNumKeys(num)
{
	var obj = window.document.getElementById(num);
	if (sel[num]) {
		obj.className  = "";
		sel[num] = 0;
		return;
	}
	sel[num] = 1;
	obj.className  = "selected";
}

function keyHandler(e)
{
	switch (e.which) {
	case key_pin:
		for (i in sel) {
			if (sel[i]) {
				Pin(window.document.getElementById(i).name);
			}
		}
		location.reload(true);
		break;
	case key_remove:
		for (i = sel.length - 1; i >= 0; i--) {
			if (sel[i]) {
				Remove(window.document.getElementById(i).name);
			}
		}
		location.reload(true);
		break;
	default:
		return false;
	}
	return true;
}
</script>
</head>

<body>
<div id="rootdiv">
<xsl:for-each select="item">
    <li><a><xsl:attribute name="id"><xsl:value-of select="position()-1"/></xsl:attribute>
	<xsl:attribute name="name"><xsl:value-of select="@index"/></xsl:attribute>
	<xsl:attribute name="onclick">processInput('<xsl:value-of select="@index"/>')</xsl:attribute>
	<xsl:value-of select="position()-1"/><xsl:text> </xsl:text><xsl:value-of select="title"/></a></li>
</xsl:for-each>

<p><a href="/vod.html">返回</a></p>


<form id="mpfrm" name="form1" method="post" target="mpdframe">
<input type="hidden" id="mpaction" name="action"/>
<input type="hidden" id="mparg" name="arg"/>
<input type="hidden" id="mpstream" name="stream" />
<input type="hidden" id="mptitle" name="title" />
</form>
<iframe id="mpdframe" name="vodxml" width="600" height="300" style="display:none"></iframe>
</div>
</div>
</body>
</html>

</xsl:template>
</xsl:stylesheet>