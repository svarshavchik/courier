<?xml version='1.0'?>
<xsl:stylesheet  
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<xsl:include href="/usr/share/sgml/docbook/xsl-stylesheets/xhtml/chunk.xsl"/>

<xsl:param name="html.stylesheet" select="'style.css'"/>
<xsl:param name="use.id.as.filename" select="1"/>
<xsl:param name="admon.graphics" select="0"/>

<xsl:param name="chunk.first.sections" select="1" />
<xsl:param name="part.autolabel" select="0" />
<xsl:param name="section.autolabel" select="0" />
<xsl:param name="chapter.autolabel" select="0" />
<xsl:param name="suppress.header.navigation" select="1" />

<xsl:template name="user.head.content">

   <meta name="MSSmartTagsPreventParsing" content="TRUE" />
   <link rel="icon" href="icon.gif" type="image/gif" />
    <xsl:comment>

Copyright 2007-2008 Double Precision, Inc.  See COPYING for distribution
information.

</xsl:comment>
</xsl:template>

</xsl:stylesheet>
