<!-- Generate a Devhelp index from the Telepathy specification.
The master copy of this stylesheet is in the Telepathy spec repository -
please make any changes there.

Copyright (C) 2006-2008 Collabora Limited

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
-->

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:tp="http://telepathy.freedesktop.org/wiki/DbusSpec#extensions-v0">
  <xsl:template match="/">
    <book xmlns="http://www.devhelp.net/book" title="Telepathy Specification"
        name="telepathy-spec" link="spec.html">
      <xsl:text>&#x000a;</xsl:text>
      <chapters>
        <xsl:text>&#x000a;</xsl:text>
        <xsl:apply-templates select="//interface" />
      </chapters>
      <xsl:text>&#x000a;</xsl:text>
      <functions>
        <xsl:text>&#x000a;</xsl:text>
        <xsl:apply-templates select="//method" />
        <xsl:apply-templates select="//signal" />
        <xsl:apply-templates select="//property" />
        <xsl:apply-templates select="//tp:enum" />
        <xsl:apply-templates select="//tp:simple-type" />
        <xsl:apply-templates select="//tp:mapping" />
        <xsl:apply-templates select="//tp:flags" />
        <xsl:apply-templates select="//tp:struct" />
      </functions>
      <xsl:text>&#x000a;</xsl:text>
    </book>
  </xsl:template>

  <xsl:template match="interface">
    <xsl:text>  </xsl:text>
    <sub xmlns="http://www.devhelp.net/book" name="{@name}"
      link="{concat('spec.html#', @name)}" />
    <xsl:text>&#x000a;</xsl:text>
  </xsl:template>

  <xsl:template match="method">
    <xsl:text>  </xsl:text>
    <keyword type="function" xmlns="http://www.devhelp.net/book" name="{@name}"
      link="spec.html#{../@name}.{@name}" />
    <xsl:text>&#x000a;</xsl:text>
  </xsl:template>

  <xsl:template match="signal | property">
    <xsl:text>  </xsl:text>
    <keyword type="" xmlns="http://www.devhelp.net/book" name="{@name}"
      link="spec.html#{../@name}.{@name}" />
    <xsl:text>&#x000a;</xsl:text>
  </xsl:template>

  <xsl:template match="tp:simple-type">
    <xsl:text>  </xsl:text>
    <keyword type="typedef" xmlns="http://www.devhelp.net/book" name="{@name}"
      link="spec.html#type-{@name}" />
    <xsl:text>&#x000a;</xsl:text>
  </xsl:template>

  <xsl:template match="tp:enum | tp:flags">
    <xsl:text>  </xsl:text>
    <keyword type="enum" xmlns="http://www.devhelp.net/book" name="{@name}"
      link="spec.html#type-{@name}" />
    <xsl:text>&#x000a;</xsl:text>
  </xsl:template>

  <xsl:template match="tp:mapping | tp:struct">
    <xsl:text>  </xsl:text>
    <keyword type="struct" xmlns="http://www.devhelp.net/book" name="{@name}"
      link="spec.html#type-{@name}" />
    <xsl:text>&#x000a;</xsl:text>
  </xsl:template>

</xsl:stylesheet>
