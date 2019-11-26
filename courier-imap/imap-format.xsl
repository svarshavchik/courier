<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
  xmlns:xhtml="http://www.w3.org/1999/xhtml">

<xsl:output method='xml' omit-xml-declaration='yes' encoding='ascii' />

<xsl:template match="xhtml:h1[@class='title']">
</xsl:template>

<xsl:template match="xhtml:head">
  <xsl:copy>
    <xsl:apply-templates select="@*"/>
    <xsl:apply-templates select="node()"/>

    <style type="text/css">
      body { font-family:sans-serif; line-height:1.2; margin:0 auto; width:60em; background-image:url(bg.png) }
      h1,h2,h3 {line-height:1.7}
      a { text-decoration:none }
      a,a:visited { color: #0000FF; }
      a:hover { text-decoration:underline }
      .gplus,footer { text-align:center }
      .advert{ clear:both }
      .copyright{ color:#7F7F7F; font-size:75%; font-style:italic }
      .gplus,.flag{float:right;margin-left:.5em}
      .toc{margin-left:2em}
      @media only screen and (max-width:60em)
      {
      body{width:92%}
      }
    </style>

      <script type="text/javascript">
        <xsl:text>
myname=self.location.href;

x=top.location.href;

if (x != myname)
{
   top.location=myname;
}
	</xsl:text>
      </script>
      <link rel="icon" href="icon.gif" type="image/gif" />
  </xsl:copy>
</xsl:template>

<xsl:template match="xhtml:body">
  <xsl:copy>
    <xsl:apply-templates select="@*"/>

    <xsl:element name="header">

      <xsl:element name="h1">
	<xsl:element name="a">
	  <xsl:attribute name="href">/imap</xsl:attribute>
	  <xsl:element name="img">
	    <xsl:attribute name="src">courier-imap.png</xsl:attribute>
	    <xsl:attribute name="width">163</xsl:attribute>
	    <xsl:attribute name="height">62</xsl:attribute>
	    <xsl:attribute name="alt">The Courier IMAP server</xsl:attribute>
	    <xsl:attribute name="title">The Courier IMAP server</xsl:attribute>
	  </xsl:element>
	</xsl:element>
      </xsl:element>

      <xsl:element name="nav">
	<xsl:attribute name="class">topnav</xsl:attribute>

	<xsl:element name="a">
	  <xsl:attribute name="href">features.html</xsl:attribute>
	  <xsl:text>Features</xsl:text>
	</xsl:element>

	<xsl:text> | </xsl:text>

	<xsl:element name="a">
	  <xsl:attribute name="href">download.html</xsl:attribute>
	  <xsl:text>Download</xsl:text>
	</xsl:element>

	<xsl:text> | </xsl:text>

	<xsl:element name="a">
	  <xsl:attribute name="href">documentation.html</xsl:attribute>
	  <xsl:text>Documentation</xsl:text>
	</xsl:element>

	<xsl:text> | </xsl:text>

	<xsl:element name="a">
	  <xsl:attribute name="href">links.html</xsl:attribute>
	  <xsl:text>Links</xsl:text>
	</xsl:element>
      </xsl:element>
    </xsl:element>

    <xsl:element name="table">
      <xsl:attribute name="class">advert</xsl:attribute>
      <xsl:element name="tbody">
        <xsl:element name="tr">
          <xsl:element name="td">
            <xsl:attribute name="width">100%</xsl:attribute>
            <xsl:text> </xsl:text>
          </xsl:element>
          <xsl:element name="td">
	    <xsl:element name="script">
	      <xsl:attribute name="async">async</xsl:attribute>
	      <xsl:attribute name="src">//pagead2.googlesyndication.com/pagead/js/adsbygoogle.js</xsl:attribute>
	      <xsl:text>&#32;</xsl:text>
	    </xsl:element>
	    <xsl:element name="ins">
	      <xsl:attribute name="class">adsbygoogle</xsl:attribute>
	      <xsl:attribute name="style">display:inline-block;width:728px;height:90px</xsl:attribute>
	      <xsl:attribute name="data-ad-client">ca-pub-3196826191648604</xsl:attribute>
	      <xsl:attribute name="data-ad-slot">1265524507</xsl:attribute>
		  <xsl:text>&#32;</xsl:text>
	    </xsl:element>

	    <xsl:element name="script">
	      <xsl:text>(adsbygoogle = window.adsbygoogle || []).push({});</xsl:text>
	    </xsl:element>
          </xsl:element>
        </xsl:element>
      </xsl:element>
    </xsl:element>

    <xsl:apply-templates select="node()"/>

  </xsl:copy>
</xsl:template>

<xsl:template match="xhtml:p[xhtml:a[starts-with(@id,'g-a-start')]]">

   <xsl:comment> google_ad_section_start </xsl:comment>

</xsl:template>

<xsl:template match="xhtml:p[xhtml:a[starts-with(@id,'g-a-end')]]">

   <xsl:comment> google_ad_section_end </xsl:comment>

</xsl:template>

<xsl:template match="@*|node()">
  <xsl:copy>
    <xsl:apply-templates select="@*"/>
    <xsl:apply-templates select="node()"/>
  </xsl:copy>
</xsl:template>

</xsl:stylesheet>
