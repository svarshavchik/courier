<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
  xmlns:xhtml="http://www.w3.org/1999/xhtml">

<xsl:output method='xml' />

<xsl:template match="xhtml:head">
  <xsl:copy>
    <xsl:apply-templates select="@*"/>
    <xsl:apply-templates select="node()"/>
      <link href="style.css" rel="stylesheet" type="text/css" />
      <script type="text/javascript" src="frame.js">
         <xsl:text> </xsl:text>
      </script>
  </xsl:copy>
</xsl:template>

<xsl:template match="xhtml:body">
  <xsl:copy>
    <xsl:apply-templates select="@*"/>

    <iframe src="header.html" frameborder="0" width="100%" height="30"
            allowtransparency="true" scrolling="no"
            style="position: relative; left: 0px; top: 0px;">
	<xsl:text> </xsl:text>
    </iframe>

    <xsl:apply-templates select="node()"/>

    <iframe src="footer.html" width="100%" allowtransparency="true"
     scrolling="no"
     frameborder="0" height="100">
	<xsl:text> </xsl:text>
    </iframe>
  </xsl:copy>
</xsl:template>

<xsl:template match="@*|node()">
  <xsl:copy>
    <xsl:apply-templates select="@*"/>
    <xsl:apply-templates select="node()"/>
  </xsl:copy>
</xsl:template>

</xsl:stylesheet>
