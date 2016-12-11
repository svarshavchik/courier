<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
		xmlns:xhtml="http://www.w3.org/1999/xhtml"
		xmlns="http://www.w3.org/1999/xhtml">

<xsl:output method='xml'
	    doctype-public="-//W3C//DTD XHTML 1.0 Transitional//EN"

	    doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"

	    />

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
      <script type="text/javascript" src=
         "https://apis.google.com/js/plusone.js">
         <xsl:text> </xsl:text>
      </script>
  </xsl:copy>
</xsl:template>

<xsl:template match="xhtml:body">
  <xsl:copy>
    <xsl:apply-templates select="@*"/>

    <xsl:element name="header">

      <xsl:element name="h1">
	<xsl:element name="a">
	  <xsl:attribute name="href">/</xsl:attribute>
	  <xsl:text>Courier Mail Server</xsl:text>
	</xsl:element>
      </xsl:element>

      <xsl:element name="nav">
	<xsl:attribute name="class">topnav</xsl:attribute>

	<xsl:element name="a">
	  <xsl:attribute name="href">status.html</xsl:attribute>
	  <xsl:text>Download</xsl:text>
	</xsl:element>

	<xsl:text> | </xsl:text>

	<xsl:element name="a">
	  <xsl:attribute name="href">documentation.html</xsl:attribute>
	  <xsl:text>Docs</xsl:text>
	</xsl:element>

	<xsl:text> | </xsl:text>

	<xsl:element name="a">
	  <xsl:attribute name="href">links.html</xsl:attribute>
	  <xsl:text>Links</xsl:text>
	</xsl:element>

	<xsl:text> | </xsl:text>

	<xsl:element name="a">
	  <xsl:attribute name="href">imap/</xsl:attribute>
	  <xsl:text>IMAP</xsl:text>
	</xsl:element>

	<xsl:text> | </xsl:text>

	<xsl:element name="a">
	  <xsl:attribute name="href">authlib/</xsl:attribute>
	  <xsl:text>Authlib</xsl:text>
	</xsl:element>

	<xsl:text> | </xsl:text>

	<xsl:element name="a">
	  <xsl:attribute name="href">maildrop/</xsl:attribute>
	  <xsl:text>Maildrop</xsl:text>
	</xsl:element>

	<xsl:text> | </xsl:text>

	<xsl:element name="a">
	  <xsl:attribute name="href">sqwebmail/</xsl:attribute>
	  <xsl:text>SqWebMail</xsl:text>
	</xsl:element>

	<xsl:text disable-output-escaping="yes">
&lt;span class="gplus"&gt;&lt;script type="text/javascript"&gt;
//&lt;![CDATA[
document.write('&lt;g:plusone href="http://www.courier-mta.org"&gt; &lt;\/g:plusone&gt;');
//]]&gt;
&lt;/script&gt;&lt;/span&gt;
	</xsl:text>
      </xsl:element>
    </xsl:element>

    <xsl:choose>
      <xsl:when test="//xhtml:div[@class='banner']">
      </xsl:when>
      <xsl:otherwise>
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
		  <xsl:attribute name="type">text/javascript</xsl:attribute>
		  <xsl:comment>
google_ad_client = "ca-pub-3196826191648604";
/* courier */
google_ad_slot = "8438116248";
google_ad_width = 728;
google_ad_height = 90;
		  </xsl:comment>
		</xsl:element>

		<xsl:element name="script">
		  <xsl:attribute name="type">text/javascript</xsl:attribute>
		  <xsl:attribute name="src">http://pagead2.googlesyndication.com/pagead/show_ads.js</xsl:attribute>
		  <xsl:text> </xsl:text>
		</xsl:element>
              </xsl:element>
            </xsl:element>
	  </xsl:element>
	</xsl:element>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:apply-templates select="node()"/>
  </xsl:copy>
</xsl:template>

<xsl:template match="@href">
  <xsl:attribute name="href">
    <xsl:if test=". = 'makeuserdb.html' or . = 'userdb.html' or . ='authlib.html'">
      <xsl:text>authlib/</xsl:text>
    </xsl:if>
    <xsl:value-of select="." />
  </xsl:attribute>
</xsl:template>

<xsl:template match="@*|node()">
  <xsl:copy>
    <xsl:apply-templates select="@*"/>
    <xsl:apply-templates select="node()"/>
  </xsl:copy>
</xsl:template>

</xsl:stylesheet>
