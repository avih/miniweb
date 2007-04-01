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
<xsl:template match="/response">

<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=gb2312"/>
<title>VOD</title>
<link href="/vod.css" rel="stylesheet" type="text/css"/>
<script language="javascript" src="/vodcfg.js"></script>
<script language="javascript" src="/vod.js"></script>
<script language="javascript">
function onNumKeys(num)
{
	Add(window.document.getElementById(num));
}
</script>
</head>

<body>
<div id="rootdiv">
<div><strong><xsl:value-of select="category/item/chars"/>字歌</strong></div>
<xsl:for-each select="category">
  <xsl:for-each select="item">
    <li><a><xsl:attribute name="id"><xsl:value-of select="@pos"/></xsl:attribute>
	<xsl:attribute name="name"><xsl:value-of select="@id"/></xsl:attribute>
	<xsl:attribute name="onclick">javascript:Add(this)</xsl:attribute>
	<xsl:value-of select="@pos"/><xsl:text> </xsl:text><xsl:value-of select="name"/>(<xsl:value-of select="../@name"/>)</a></li>
  </xsl:for-each>
</xsl:for-each>

<div style="display:block">
<p>
    <input name="button22" type="button" onclick="Command(document.getElementById('cmd').value)" value="Send Command"/>
    <input type="text" name="textfield" id="cmd"/>
</p>
<form id="mpfrm" name="form1" method="post" target="mpdframe">
<input type="hidden" id="mpaction" name="action"/>
<input type="hidden" id="mparg" name="arg"/>
<input type="hidden" id="mpstream" name="stream" />
<input type="hidden" id="mptitle" name="title" />
</form>
<p>&nbsp;</p>
<iframe id="mpdframe" name="vodxml" width="600" height="300" style="display:block"></iframe>
</div>
</div>
</body>
</html>

</xsl:template>
</xsl:stylesheet>