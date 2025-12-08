<?xml version='1.0'?>
<xsl:stylesheet
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<xsl:include href="http://docbook.sourceforge.net/release/xsl/current/xhtml/chunk.xsl"/>

<xsl:param name="html.stylesheet" select="'manpage.css'"/>
<xsl:param name="admon.graphics" select="0"/>

<xsl:param name="use.id.as.filename" select="1"/>

<xsl:param name="funcsynopsis.style">ansi</xsl:param>
<xsl:param name="table.borders.with.css" select="1" />

<xsl:param name="default.table.frame" select="'collapse'" />
<xsl:param name="table.cell.border.style" select="''" />
<xsl:param name="table.cell.border.thickness" select="''" />
<xsl:param name="table.cell.border.color" select="''" />
<xsl:param name="emphasis.propagates.style" select="1" />
<xsl:param name="para.propagates.style" select="1" />
<xsl:param name="entry.propagates.style" select="1" />

<xsl:param name="part.autolabel" select="0" />
<xsl:param name="section.autolabel" select="0" />
<xsl:param name="chapter.autolabel" select="0" />

<xsl:template name="user.head.content">

  <link rel="icon" href="icon.gif" type="image/gif"/>

   <meta name="MSSmartTagsPreventParsing" content="TRUE" />
    <xsl:comment>

Copyright 2002 - 2022 S. Varshavchik.  See COPYING for distribution
information.

</xsl:comment>
</xsl:template>

<xsl:param name="chunk.tocs.and.lots" select="1" />
<xsl:param name="generate.toc" select="'book toc'"/>

</xsl:stylesheet>
