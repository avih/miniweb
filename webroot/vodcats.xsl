<?xml version="1.0" encoding="utf-8"?><!-- DWXMLSource="vodcat.xsl" --><!DOCTYPE xsl:stylesheet  [
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
<link href="/vod.css" rel="stylesheet" type="text/css"/>
<script language="javascript" src="/vodcfg.js"></script>
<script language="javascript" src="/vod.js"></script>
<script language="javascript">
function onNumKeys(num)
{
	Go(document.getElementById(num).href, vod_cats_url);
}
</script>
</head>

<body>
<div><strong>歌手点歌</strong></div>
<xsl:for-each select="category">
<xsl:if test="name != ''">
<li><xsl:value-of select="position()-1"/>&nbsp;
<a><xsl:attribute name="id"><xsl:value-of select="position()-1"/></xsl:attribute>
<xsl:attribute name="href">title?xsl=/vodcatslist.xsl&amp;catid=<xsl:value-of select="@id"/>&amp;count=10</xsl:attribute>
<xsl:value-of select="name"/></a> (<xsl:value-of select="clips"/>首)</li>
</xsl:if>
</xsl:for-each>
</body>
</html>

</xsl:template>
</xsl:stylesheet>