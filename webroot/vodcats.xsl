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

<xsl:for-each select="category">
<xsl:if test="name != ''">
<li><xsl:value-of select="position()-1"/>&nbsp;
<xsl:value-of select="name"/>&nbsp;<span style="font-size:small"><xsl:value-of select="clips"/>首</span></li>
</xsl:if>
</xsl:for-each>

</xsl:template>
</xsl:stylesheet>