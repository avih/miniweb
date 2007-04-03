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
<xsl:output method="html" encoding="gb2312"/>
<xsl:template match="/response">

<div id="title" style="font-weight:bold"><xsl:value-of select="category/@name"/></div>

<xsl:for-each select="category">
<xsl:for-each select="item">
<li><xsl:attribute name="id"><xsl:value-of select="@pos"/></xsl:attribute>
<xsl:value-of select="@pos"/>&nbsp;<xsl:value-of select="name"/>
<xsl:if test="../@name != ''">
<span class="list_postfix">(<xsl:value-of select="../@name"/>)</span>
</xsl:if>
</li>
</xsl:for-each>
</xsl:for-each>

</xsl:template>
</xsl:stylesheet>