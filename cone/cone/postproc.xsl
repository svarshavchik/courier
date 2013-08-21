<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
  xmlns:xhtml="http://www.w3.org/1999/xhtml">

<xsl:output method='xml' encoding='ascii' omit-xml-declaration='yes' />

<xsl:template match="/xhtml:html/xhtml:head/xhtml:meta[@name='generator']">

</xsl:template>

<!-- Zap empty <a> nodes -->

<xsl:template match="xhtml:a">

  <xsl:copy>
    <xsl:apply-templates select="@*|node()"/>
    <xsl:if test="count(child::node()) = 0">
      <xsl:text> </xsl:text>
    </xsl:if>
  </xsl:copy>

</xsl:template>

<xsl:template match="xhtml:script">

  <xsl:copy>
    <xsl:apply-templates select="@*|node()"/>
    <xsl:if test="count(child::node()) = 0">
      <xsl:text> </xsl:text>
    </xsl:if>
  </xsl:copy>

</xsl:template>

<xsl:template match="@*|node()">
  <xsl:copy>
    <xsl:apply-templates select="@*|node()"/>
  </xsl:copy>
</xsl:template>

</xsl:stylesheet>
