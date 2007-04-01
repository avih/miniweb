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

<xsl:for-each select="category">
  <li><xsl:value-of select="position()-1"/>&nbsp;
  <xsl:if test="@chars = '0'">所有歌曲</xsl:if>
  <xsl:if test="@chars != '0'">
  <xsl:value-of select="@chars"/>字歌 <span style="font-size:small">(<xsl:value-of select="@count"/>首)</span>
  </xsl:if>
  </li>
</xsl:for-each>

</xsl:template>
</xsl:stylesheet>