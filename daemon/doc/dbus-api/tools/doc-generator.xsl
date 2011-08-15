<!-- Generate HTML documentation from the Telepathy specification.
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

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:tp="http://telepathy.freedesktop.org/wiki/DbusSpec#extensions-v0"
  xmlns:html="http://www.w3.org/1999/xhtml"
  exclude-result-prefixes="tp html">
  <!--Don't move the declaration of the HTML namespace up here — XMLNSs
  don't work ideally in the presence of two things that want to use the
  absence of a prefix, sadly. -->

  <xsl:param name="allow-undefined-interfaces" select="false()"/>

  <xsl:template match="html:* | @*" mode="html">
    <xsl:copy>
      <xsl:apply-templates mode="html" select="@*|node()"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="tp:type" mode="html">
    <xsl:call-template name="tp-type">
      <xsl:with-param name="tp-type" select="string(.)"/>
    </xsl:call-template>
  </xsl:template>

  <!-- tp:dbus-ref: reference a D-Bus interface, signal, method or property -->
  <xsl:template match="tp:dbus-ref" mode="html">
    <xsl:variable name="name">
      <xsl:choose>
        <xsl:when test="@namespace">
          <xsl:value-of select="@namespace"/>
          <xsl:text>.</xsl:text>
        </xsl:when>
      </xsl:choose>
      <xsl:value-of select="string(.)"/>
    </xsl:variable>

    <xsl:choose>
      <xsl:when test="//interface[@name=$name]
        or //interface/method[concat(../@name, '.', @name)=$name]
        or //interface/signal[concat(../@name, '.', @name)=$name]
        or //interface/property[concat(../@name, '.', @name)=$name]
        or //interface[@name=concat($name, '.DRAFT')]
        or //interface/method[
          concat(../@name, '.', @name)=concat($name, '.DRAFT')]
        or //interface/signal[
          concat(../@name, '.', @name)=concat($name, '.DRAFT')]
        or //interface/property[
          concat(../@name, '.', @name)=concat($name, '.DRAFT')]
        ">
        <a xmlns="http://www.w3.org/1999/xhtml" href="#{$name}">
          <xsl:value-of select="string(.)"/>
        </a>
      </xsl:when>

      <xsl:when test="$allow-undefined-interfaces">
        <span xmlns="http://www.w3.org/1999/xhtml" title="defined elsewhere">
          <xsl:value-of select="string(.)"/>
        </span>
      </xsl:when>

      <xsl:otherwise>
        <xsl:message terminate="yes">
          <xsl:text>ERR: cannot find D-Bus interface, method, </xsl:text>
          <xsl:text>signal or property called '</xsl:text>
          <xsl:value-of select="$name"/>
          <xsl:text>'&#10;</xsl:text>
        </xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- tp:member-ref: reference a property of the current interface -->
  <xsl:template match="tp:member-ref" mode="html">
    <xsl:variable name="prefix" select="concat(ancestor::interface/@name,
      '.')"/>
    <xsl:variable name="name" select="string(.)"/>

    <xsl:if test="not(ancestor::interface)">
      <xsl:message terminate="yes">
        <xsl:text>ERR: Cannot use tp:member-ref when not in an</xsl:text>
        <xsl:text> &lt;interface&gt;&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:choose>
      <xsl:when test="ancestor::interface/signal[@name=$name]"/>
      <xsl:when test="ancestor::interface/method[@name=$name]"/>
      <xsl:when test="ancestor::interface/property[@name=$name]"/>
      <xsl:otherwise>
        <xsl:message terminate="yes">
          <xsl:text>ERR: interface </xsl:text>
          <xsl:value-of select="ancestor::interface/@name"/>
          <xsl:text> has no signal/method/property called </xsl:text>
          <xsl:value-of select="$name"/>
          <xsl:text>&#10;</xsl:text>
        </xsl:message>
      </xsl:otherwise>
    </xsl:choose>

    <a xmlns="http://www.w3.org/1999/xhtml" href="#{$prefix}{$name}">
      <xsl:value-of select="$name"/>
    </a>
  </xsl:template>

  <xsl:template match="*" mode="identity">
    <xsl:copy>
      <xsl:apply-templates mode="identity"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="tp:docstring">
    <xsl:apply-templates mode="html"/>
  </xsl:template>

  <xsl:template match="tp:added">
    <p class="added" xmlns="http://www.w3.org/1999/xhtml">Added in
      version <xsl:value-of select="@version"/>.
      <xsl:apply-templates select="node()" mode="html"/></p>
  </xsl:template>

  <xsl:template match="tp:changed">
    <xsl:choose>
      <xsl:when test="node()">
        <p class="changed" xmlns="http://www.w3.org/1999/xhtml">Changed in
          version <xsl:value-of select="@version"/>:
          <xsl:apply-templates select="node()" mode="html"/></p>
      </xsl:when>
      <xsl:otherwise>
        <p class="changed">Changed in version
          <xsl:value-of select="@version"/></p>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="tp:deprecated">
    <p class="deprecated" xmlns="http://www.w3.org/1999/xhtml">Deprecated
      since version <xsl:value-of select="@version"/>.
      <xsl:apply-templates select="node()" mode="html"/></p>
  </xsl:template>

  <xsl:template match="tp:rationale" mode="html">
    <div xmlns="http://www.w3.org/1999/xhtml" class="rationale">
      <xsl:apply-templates select="node()" mode="html"/>
    </div>
  </xsl:template>

  <xsl:template match="tp:errors">
    <h1 xmlns="http://www.w3.org/1999/xhtml">Errors</h1>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="tp:generic-types">
    <h1 xmlns="http://www.w3.org/1999/xhtml">Generic types</h1>
    <xsl:call-template name="do-types"/>
  </xsl:template>

  <xsl:template name="do-types">
    <xsl:if test="tp:simple-type">
      <h2 xmlns="http://www.w3.org/1999/xhtml">Simple types</h2>
      <xsl:apply-templates select="tp:simple-type"/>
    </xsl:if>

    <xsl:if test="tp:enum">
      <h2 xmlns="http://www.w3.org/1999/xhtml">Enumerated types:</h2>
      <xsl:apply-templates select="tp:enum"/>
    </xsl:if>

    <xsl:if test="tp:flags">
      <h2 xmlns="http://www.w3.org/1999/xhtml">Sets of flags:</h2>
      <xsl:apply-templates select="tp:flags"/>
    </xsl:if>

    <xsl:if test="tp:struct">
      <h2 xmlns="http://www.w3.org/1999/xhtml">Structure types</h2>
      <xsl:apply-templates select="tp:struct"/>
    </xsl:if>

    <xsl:if test="tp:mapping">
      <h2 xmlns="http://www.w3.org/1999/xhtml">Mapping types</h2>
      <xsl:apply-templates select="tp:mapping"/>
    </xsl:if>

    <xsl:if test="tp:external-type">
      <h2 xmlns="http://www.w3.org/1999/xhtml">Types defined elsewhere</h2>
      <dl><xsl:apply-templates select="tp:external-type"/></dl>
    </xsl:if>
  </xsl:template>

  <xsl:template match="tp:error">
    <h2 xmlns="http://www.w3.org/1999/xhtml"><a name="{concat(../@namespace, '.', translate(@name, ' ', ''))}"></a><xsl:value-of select="concat(../@namespace, '.', translate(@name, ' ', ''))"/></h2>
    <xsl:apply-templates select="tp:docstring"/>
    <xsl:apply-templates select="tp:added"/>
    <xsl:apply-templates select="tp:changed"/>
    <xsl:apply-templates select="tp:deprecated"/>
  </xsl:template>

  <xsl:template match="/tp:spec/tp:copyright">
    <div xmlns="http://www.w3.org/1999/xhtml">
      <xsl:apply-templates mode="text"/>
    </div>
  </xsl:template>
  <xsl:template match="/tp:spec/tp:license">
    <div xmlns="http://www.w3.org/1999/xhtml" class="license">
      <xsl:apply-templates mode="html"/>
    </div>
  </xsl:template>

  <xsl:template match="tp:copyright"/>
  <xsl:template match="tp:license"/>

  <xsl:template match="interface">
    <h1 xmlns="http://www.w3.org/1999/xhtml"><a name="{@name}"></a><xsl:value-of select="@name"/></h1>

    <xsl:if test="@tp:causes-havoc">
      <p xmlns="http://www.w3.org/1999/xhtml" class="causes-havoc">
        This interface is <xsl:value-of select="@tp:causes-havoc"/>
        and is likely to cause havoc to your API/ABI if bindings are generated.
        Don't include it in libraries that care about compatibility.
      </p>
    </xsl:if>

    <xsl:if test="tp:requires">
      <p>Implementations of this interface must also implement:</p>
      <ul xmlns="http://www.w3.org/1999/xhtml">
        <xsl:for-each select="tp:requires">
          <li><code><a href="#{@interface}"><xsl:value-of select="@interface"/></a></code></li>
        </xsl:for-each>
      </ul>
    </xsl:if>

    <xsl:apply-templates select="tp:docstring" />
    <xsl:apply-templates select="tp:added"/>
    <xsl:apply-templates select="tp:changed"/>
    <xsl:apply-templates select="tp:deprecated"/>

    <xsl:choose>
      <xsl:when test="method">
        <h2 xmlns="http://www.w3.org/1999/xhtml">Methods:</h2>
        <xsl:apply-templates select="method"/>
      </xsl:when>
      <xsl:otherwise>
        <p xmlns="http://www.w3.org/1999/xhtml">Interface has no methods.</p>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:choose>
      <xsl:when test="signal">
        <h2 xmlns="http://www.w3.org/1999/xhtml">Signals:</h2>
        <xsl:apply-templates select="signal"/>
      </xsl:when>
      <xsl:otherwise>
        <p xmlns="http://www.w3.org/1999/xhtml">Interface has no signals.</p>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:choose>
      <xsl:when test="tp:property">
        <h2 xmlns="http://www.w3.org/1999/xhtml">Telepathy Properties:</h2>
        <p xmlns="http://www.w3.org/1999/xhtml">Accessed using the
          <a href="#org.freedesktop.Telepathy.Properties">Telepathy
            Properties</a> interface.</p>
        <dl xmlns="http://www.w3.org/1999/xhtml">
          <xsl:apply-templates select="tp:property"/>
        </dl>
      </xsl:when>
      <xsl:otherwise>
        <p xmlns="http://www.w3.org/1999/xhtml">Interface has no Telepathy
          properties.</p>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:choose>
      <xsl:when test="property">
        <h2 xmlns="http://www.w3.org/1999/xhtml">D-Bus core Properties:</h2>
        <p xmlns="http://www.w3.org/1999/xhtml">Accessed using the
          org.freedesktop.DBus.Properties interface.</p>
        <dl xmlns="http://www.w3.org/1999/xhtml">
          <xsl:apply-templates select="property"/>
        </dl>
      </xsl:when>
      <xsl:otherwise>
        <p xmlns="http://www.w3.org/1999/xhtml">Interface has no D-Bus core
          properties.</p>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:call-template name="do-types"/>

  </xsl:template>

  <xsl:template match="tp:flags">

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a tp:flags type&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@type) or @type = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @type on tp:flags type</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <h3>
      <a name="type-{@name}">
        <xsl:value-of select="@name"/>
      </a>
    </h3>
    <xsl:apply-templates select="tp:docstring" />
    <xsl:apply-templates select="tp:added"/>
    <xsl:apply-templates select="tp:changed"/>
    <xsl:apply-templates select="tp:deprecated"/>
    <dl xmlns="http://www.w3.org/1999/xhtml">
        <xsl:variable name="value-prefix">
          <xsl:choose>
            <xsl:when test="@value-prefix">
              <xsl:value-of select="@value-prefix"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="@name"/>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
      <xsl:for-each select="tp:flag">
        <dt xmlns="http://www.w3.org/1999/xhtml"><code><xsl:value-of select="concat($value-prefix, '_', @suffix)"/> = <xsl:value-of select="@value"/></code></dt>
        <xsl:choose>
          <xsl:when test="tp:docstring">
            <dd xmlns="http://www.w3.org/1999/xhtml">
              <xsl:apply-templates select="tp:docstring" />
              <xsl:apply-templates select="tp:added"/>
              <xsl:apply-templates select="tp:changed"/>
              <xsl:apply-templates select="tp:deprecated"/>
            </dd>
          </xsl:when>
          <xsl:otherwise>
            <dd xmlns="http://www.w3.org/1999/xhtml">(Undocumented)</dd>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:for-each>
    </dl>
  </xsl:template>

  <xsl:template match="tp:enum">

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a tp:enum type&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@type) or @type = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @type on tp:enum type</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <h3 xmlns="http://www.w3.org/1999/xhtml">
      <a name="type-{@name}">
        <xsl:value-of select="@name"/>
      </a>
    </h3>
    <xsl:apply-templates select="tp:docstring" />
    <xsl:apply-templates select="tp:added"/>
    <xsl:apply-templates select="tp:changed"/>
    <xsl:apply-templates select="tp:deprecated"/>
    <dl xmlns="http://www.w3.org/1999/xhtml">
        <xsl:variable name="value-prefix">
          <xsl:choose>
            <xsl:when test="@value-prefix">
              <xsl:value-of select="@value-prefix"/>
            </xsl:when>
            <xsl:otherwise>
              <xsl:value-of select="@name"/>
            </xsl:otherwise>
          </xsl:choose>
        </xsl:variable>
      <xsl:for-each select="tp:enumvalue">
        <dt xmlns="http://www.w3.org/1999/xhtml"><code><xsl:value-of select="concat($value-prefix, '_', @suffix)"/> = <xsl:value-of select="@value"/></code></dt>
        <xsl:choose>
          <xsl:when test="tp:docstring">
            <dd xmlns="http://www.w3.org/1999/xhtml">
              <xsl:apply-templates select="tp:docstring" />
              <xsl:apply-templates select="tp:added"/>
              <xsl:apply-templates select="tp:changed"/>
              <xsl:apply-templates select="tp:deprecated"/>
            </dd>
          </xsl:when>
          <xsl:otherwise>
            <dd xmlns="http://www.w3.org/1999/xhtml">(Undocumented)</dd>
          </xsl:otherwise>
        </xsl:choose>
      </xsl:for-each>
    </dl>
  </xsl:template>

  <xsl:template name="binding-name-check">
    <xsl:if test="not(@tp:name-for-bindings)">
      <xsl:message terminate="yes">
        <xsl:text>ERR: Binding name missing from </xsl:text>
        <xsl:value-of select="parent::interface/@name"/>
        <xsl:text>.</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="translate(@tp:name-for-bindings, '_', '') != @name">
      <xsl:message terminate="yes">
        <xsl:text>ERR: Binding name </xsl:text>
        <xsl:value-of select="@tp:name-for-bindings"/>
        <xsl:text> doesn't correspond to D-Bus name </xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>
  </xsl:template>

  <xsl:template match="property">

    <xsl:call-template name="binding-name-check"/>

    <xsl:if test="not(parent::interface)">
      <xsl:message terminate="yes">
        <xsl:text>ERR: property </xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text> does not have an interface as parent&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a property of </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@type) or @type = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @type on property </xsl:text>
        <xsl:value-of select="concat(../@name, '.', @name)"/>
        <xsl:text>: '</xsl:text>
        <xsl:value-of select="@access"/>
        <xsl:text>'&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <dt xmlns="http://www.w3.org/1999/xhtml">
      <a name="{concat(../@name, '.', @name)}">
        <code><xsl:value-of select="@name"/></code>
      </a>
      <xsl:text> − </xsl:text>
      <code><xsl:value-of select="@type"/></code>
      <xsl:call-template name="parenthesized-tp-type"/>
      <xsl:text>, </xsl:text>
      <xsl:choose>
        <xsl:when test="@access = 'read'">
          <xsl:text>read-only</xsl:text>
        </xsl:when>
        <xsl:when test="@access = 'write'">
          <xsl:text>write-only</xsl:text>
        </xsl:when>
        <xsl:when test="@access = 'readwrite'">
          <xsl:text>read/write</xsl:text>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
            <xsl:text>ERR: unknown or missing value for </xsl:text>
            <xsl:text>@access on property </xsl:text>
            <xsl:value-of select="concat(../@name, '.', @name)"/>
            <xsl:text>: '</xsl:text>
            <xsl:value-of select="@access"/>
            <xsl:text>'&#10;</xsl:text>
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </dt>
    <dd xmlns="http://www.w3.org/1999/xhtml">
      <xsl:apply-templates select="tp:docstring"/>
      <xsl:apply-templates select="tp:added"/>
      <xsl:apply-templates select="tp:changed"/>
      <xsl:apply-templates select="tp:deprecated"/>
    </dd>
  </xsl:template>

  <xsl:template match="tp:property">
    <dt xmlns="http://www.w3.org/1999/xhtml">
      <xsl:if test="@name">
        <code><xsl:value-of select="@name"/></code> −
      </xsl:if>
      <code><xsl:value-of select="@type"/></code>
    </dt>
    <dd xmlns="http://www.w3.org/1999/xhtml">
      <xsl:apply-templates select="tp:docstring"/>
      <xsl:apply-templates select="tp:added"/>
      <xsl:apply-templates select="tp:changed"/>
      <xsl:apply-templates select="tp:deprecated"/>
    </dd>
  </xsl:template>

  <xsl:template match="tp:mapping">
    <div xmlns="http://www.w3.org/1999/xhtml" class="struct">
      <h3>
        <a name="type-{@name}">
          <xsl:value-of select="@name"/>
        </a> − a{
        <xsl:for-each select="tp:member">
          <xsl:value-of select="@type"/>
          <xsl:text>: </xsl:text>
          <xsl:value-of select="@name"/>
          <xsl:if test="position() != last()"> &#x2192; </xsl:if>
        </xsl:for-each>
        }
      </h3>
      <div class="docstring">
        <xsl:apply-templates select="tp:docstring"/>
        <xsl:if test="string(@array-name) != ''">
          <p>In bindings that need a separate name, arrays of
            <xsl:value-of select="@name"/> should be called
            <xsl:value-of select="@array-name"/>.</p>
        </xsl:if>
      </div>
      <div>
        <h4>Members</h4>
        <dl>
          <xsl:apply-templates select="tp:member" mode="members-in-docstring"/>
        </dl>
      </div>
    </div>
  </xsl:template>

  <xsl:template match="tp:docstring" mode="in-index"/>

  <xsl:template match="tp:simple-type | tp:enum | tp:flags | tp:external-type"
    mode="in-index">
    − <xsl:value-of select="@type"/>
  </xsl:template>

  <xsl:template match="tp:simple-type">

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a tp:simple-type&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@type) or @type = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @type on tp:simple-type</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <div xmlns="http://www.w3.org/1999/xhtml" class="simple-type">
      <h3>
        <a name="type-{@name}">
          <xsl:value-of select="@name"/>
        </a> − <xsl:value-of select="@type"/>
      </h3>
      <div class="docstring">
        <xsl:apply-templates select="tp:docstring"/>
        <xsl:apply-templates select="tp:added"/>
        <xsl:apply-templates select="tp:changed"/>
        <xsl:apply-templates select="tp:deprecated"/>
      </div>
    </div>
  </xsl:template>

  <xsl:template match="tp:external-type">

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a tp:external-type&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@type) or @type = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @type on tp:external-type</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <div xmlns="http://www.w3.org/1999/xhtml" class="external-type">
      <dt>
        <a name="type-{@name}">
          <xsl:value-of select="@name"/>
        </a> − <xsl:value-of select="@type"/>
      </dt>
      <dd>Defined by: <xsl:value-of select="@from"/></dd>
    </div>
  </xsl:template>

  <xsl:template match="tp:struct" mode="in-index">
    − ( <xsl:for-each select="tp:member">
          <xsl:value-of select="@type"/>
          <xsl:if test="position() != last()">, </xsl:if>
        </xsl:for-each> )
  </xsl:template>

  <xsl:template match="tp:mapping" mode="in-index">
    − a{ <xsl:for-each select="tp:member">
          <xsl:value-of select="@type"/>
          <xsl:if test="position() != last()"> &#x2192; </xsl:if>
        </xsl:for-each> }
  </xsl:template>

  <xsl:template match="tp:struct">
    <div xmlns="http://www.w3.org/1999/xhtml" class="struct">
      <h3>
        <a name="type-{@name}">
          <xsl:value-of select="@name"/>
        </a> − (
        <xsl:for-each select="tp:member">
          <xsl:value-of select="@type"/>
          <xsl:text>: </xsl:text>
          <xsl:value-of select="@name"/>
          <xsl:if test="position() != last()">, </xsl:if>
        </xsl:for-each>
        )
      </h3>
      <div class="docstring">
        <xsl:apply-templates select="tp:docstring"/>
        <xsl:apply-templates select="tp:added"/>
        <xsl:apply-templates select="tp:changed"/>
        <xsl:apply-templates select="tp:deprecated"/>
      </div>
      <xsl:choose>
        <xsl:when test="string(@array-name) != ''">
          <p>In bindings that need a separate name, arrays of
            <xsl:value-of select="@name"/> should be called
            <xsl:value-of select="@array-name"/>.</p>
        </xsl:when>
        <xsl:otherwise>
          <p>Arrays of <xsl:value-of select="@name"/> don't generally
            make sense.</p>
        </xsl:otherwise>
      </xsl:choose>
      <div>
        <h4>Members</h4>
        <dl>
          <xsl:apply-templates select="tp:member" mode="members-in-docstring"/>
        </dl>
      </div>
    </div>
  </xsl:template>

  <xsl:template match="method">

    <xsl:call-template name="binding-name-check"/>

    <xsl:if test="not(parent::interface)">
      <xsl:message terminate="yes">
        <xsl:text>ERR: method </xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text> does not have an interface as parent&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a method of </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:for-each select="arg">
      <xsl:if test="not(@type) or @type = ''">
        <xsl:message terminate="yes">
          <xsl:text>ERR: an arg of method </xsl:text>
          <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
          <xsl:text> has no type</xsl:text>
        </xsl:message>
      </xsl:if>
      <xsl:choose>
        <xsl:when test="@direction='in'">
          <xsl:if test="not(@name) or @name = ''">
            <xsl:message terminate="yes">
              <xsl:text>ERR: an 'in' arg of method </xsl:text>
              <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
              <xsl:text> has no name</xsl:text>
            </xsl:message>
          </xsl:if>
        </xsl:when>
        <xsl:when test="@direction='out'">
          <xsl:if test="not(@name) or @name = ''">
            <xsl:message terminate="no">
              <xsl:text>WARNING: an 'out' arg of method </xsl:text>
              <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
              <xsl:text> has no name</xsl:text>
            </xsl:message>
          </xsl:if>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
            <xsl:text>ERR: an arg of method </xsl:text>
            <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
            <xsl:text> has direction neither 'in' nor 'out'</xsl:text>
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>

    <div xmlns="http://www.w3.org/1999/xhtml" class="method">
      <h3 xmlns="http://www.w3.org/1999/xhtml">
        <a name="{concat(../@name, concat('.', @name))}">
          <xsl:value-of select="@name"/>
        </a> (
        <xsl:for-each xmlns="" select="arg[@direction='in']">
          <xsl:value-of select="@type"/>: <xsl:value-of select="@name"/>
          <xsl:if test="position() != last()">, </xsl:if>
        </xsl:for-each>
        ) &#x2192;
        <xsl:choose>
          <xsl:when test="arg[@direction='out']">
            <xsl:for-each xmlns="" select="arg[@direction='out']">
              <xsl:value-of select="@type"/>
              <xsl:if test="position() != last()">, </xsl:if>
            </xsl:for-each>
          </xsl:when>
          <xsl:otherwise>nothing</xsl:otherwise>
        </xsl:choose>
      </h3>
      <div xmlns="http://www.w3.org/1999/xhtml" class="docstring">
        <xsl:apply-templates select="tp:docstring" />
        <xsl:apply-templates select="tp:added"/>
        <xsl:apply-templates select="tp:changed"/>
        <xsl:apply-templates select="tp:deprecated"/>
      </div>

      <xsl:if test="arg[@direction='in']">
        <div xmlns="http://www.w3.org/1999/xhtml">
          <h4>Parameters</h4>
          <dl xmlns="http://www.w3.org/1999/xhtml">
            <xsl:apply-templates select="arg[@direction='in']"
              mode="parameters-in-docstring"/>
          </dl>
        </div>
      </xsl:if>

      <xsl:if test="arg[@direction='out']">
        <div xmlns="http://www.w3.org/1999/xhtml">
          <h4>Returns</h4>
          <dl xmlns="http://www.w3.org/1999/xhtml">
            <xsl:apply-templates select="arg[@direction='out']"
              mode="returns-in-docstring"/>
          </dl>
        </div>
      </xsl:if>

      <xsl:if test="tp:possible-errors">
        <div xmlns="http://www.w3.org/1999/xhtml">
          <h4>Possible errors</h4>
          <dl xmlns="http://www.w3.org/1999/xhtml">
            <xsl:apply-templates select="tp:possible-errors/tp:error"/>
          </dl>
        </div>
      </xsl:if>

    </div>
  </xsl:template>

  <xsl:template name="tp-type">
    <xsl:param name="tp-type"/>
    <xsl:param name="type"/>

    <xsl:variable name="single-type">
      <xsl:choose>
        <xsl:when test="contains($tp-type, '[]')">
          <xsl:value-of select="substring-before($tp-type, '[]')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$tp-type"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:variable name="type-of-single-tp-type">
      <xsl:choose>
        <xsl:when test="//tp:simple-type[@name=$single-type]">
          <xsl:value-of select="string(//tp:simple-type[@name=$single-type]/@type)"/>
        </xsl:when>
        <xsl:when test="//tp:struct[@name=$single-type]">
          <xsl:text>(</xsl:text>
          <xsl:for-each select="//tp:struct[@name=$single-type]/tp:member">
            <xsl:value-of select="@type"/>
          </xsl:for-each>
          <xsl:text>)</xsl:text>
        </xsl:when>
        <xsl:when test="//tp:enum[@name=$single-type]">
          <xsl:value-of select="string(//tp:enum[@name=$single-type]/@type)"/>
        </xsl:when>
        <xsl:when test="//tp:flags[@name=$single-type]">
          <xsl:value-of select="string(//tp:flags[@name=$single-type]/@type)"/>
        </xsl:when>
        <xsl:when test="//tp:mapping[@name=$single-type]">
          <xsl:text>a{</xsl:text>
          <xsl:for-each select="//tp:mapping[@name=$single-type]/tp:member">
            <xsl:value-of select="@type"/>
          </xsl:for-each>
          <xsl:text>}</xsl:text>
        </xsl:when>
        <xsl:when test="//tp:external-type[@name=$single-type]">
          <xsl:value-of select="string(//tp:external-type[@name=$single-type]/@type)"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
            <xsl:text>ERR: Unable to find type '</xsl:text>
            <xsl:value-of select="$tp-type"/>
            <xsl:text>'&#10;</xsl:text>
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:variable name="type-of-tp-type">
      <xsl:if test="contains($tp-type, '[]')">
        <!-- one 'a', plus one for each [ after the [], and delete all ] -->
        <xsl:value-of select="concat('a',
          translate(substring-after($tp-type, '[]'), '[]', 'a'))"/>
      </xsl:if>
      <xsl:value-of select="$type-of-single-tp-type"/>
    </xsl:variable>

    <xsl:if test="string($type) != '' and
      string($type-of-tp-type) != string($type)">
      <xsl:message terminate="yes">
        <xsl:text>ERR: tp:type '</xsl:text>
        <xsl:value-of select="$tp-type"/>
        <xsl:text>' has D-Bus type '</xsl:text>
        <xsl:value-of select="$type-of-tp-type"/>
        <xsl:text>' but has been used with type='</xsl:text>
        <xsl:value-of select="$type"/>
        <xsl:text>'&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="contains($tp-type, '[]')">
      <xsl:call-template name="tp-type-array-usage-check">
        <xsl:with-param name="single-type" select="$single-type"/>
        <xsl:with-param name="type-of-single-tp-type"
          select="$type-of-single-tp-type"/>
      </xsl:call-template>
    </xsl:if>

    <a href="#type-{$single-type}"><xsl:value-of select="$tp-type"/></a>

  </xsl:template>

  <xsl:template name="tp-type-array-usage-check">
    <xsl:param name="single-type"/>
    <xsl:param name="type-of-single-tp-type"/>

    <xsl:variable name="array-name">
      <xsl:choose>
        <xsl:when test="//tp:struct[@name=$single-type]">
          <xsl:value-of select="//tp:struct[@name=$single-type]/@array-name"/>
        </xsl:when>
        <xsl:when test="//tp:mapping[@name=$single-type]">
          <xsl:value-of select="//tp:mapping[@name=$single-type]/@array-name"/>
        </xsl:when>
        <xsl:when test="//tp:external-type[@name=$single-type]">
          <xsl:value-of select="//tp:external-type[@name=$single-type]/@array-name"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="''"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:if test="not(contains('ybnqiuxtdsvog', $type-of-single-tp-type))">
      <xsl:if test="not($array-name) or $array-name=''">
        <xsl:message terminate="yes">
          <xsl:text>No array-name specified for complex type </xsl:text>
          <xsl:value-of select="$single-type"/>
          <xsl:text>, but array used&#10;</xsl:text>
        </xsl:message>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <xsl:template name="parenthesized-tp-type">
    <xsl:if test="@tp:type">
      <xsl:text> (</xsl:text>
      <xsl:call-template name="tp-type">
        <xsl:with-param name="tp-type" select="@tp:type"/>
        <xsl:with-param name="type" select="@type"/>
      </xsl:call-template>
      <xsl:text>)</xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="tp:member" mode="members-in-docstring">
    <dt xmlns="http://www.w3.org/1999/xhtml">
      <code><xsl:value-of select="@name"/></code> −
      <code><xsl:value-of select="@type"/></code>
      <xsl:call-template name="parenthesized-tp-type"/>
    </dt>
    <dd xmlns="http://www.w3.org/1999/xhtml">
      <xsl:choose>
        <xsl:when test="tp:docstring">
          <xsl:apply-templates select="tp:docstring" />
        </xsl:when>
        <xsl:otherwise>
          <em>(undocumented)</em>
        </xsl:otherwise>
      </xsl:choose>
    </dd>
  </xsl:template>

  <xsl:template match="arg" mode="parameters-in-docstring">
    <dt xmlns="http://www.w3.org/1999/xhtml">
      <code><xsl:value-of select="@name"/></code> −
      <code><xsl:value-of select="@type"/></code>
      <xsl:call-template name="parenthesized-tp-type"/>
    </dt>
    <dd xmlns="http://www.w3.org/1999/xhtml">
      <xsl:apply-templates select="tp:docstring" />
    </dd>
  </xsl:template>

  <xsl:template match="arg" mode="returns-in-docstring">
    <dt xmlns="http://www.w3.org/1999/xhtml">
      <xsl:if test="@name">
        <code><xsl:value-of select="@name"/></code> −
      </xsl:if>
      <code><xsl:value-of select="@type"/></code>
      <xsl:call-template name="parenthesized-tp-type"/>
    </dt>
    <dd xmlns="http://www.w3.org/1999/xhtml">
      <xsl:apply-templates select="tp:docstring"/>
    </dd>
  </xsl:template>

  <xsl:template match="tp:possible-errors/tp:error">
    <dt xmlns="http://www.w3.org/1999/xhtml">
      <code><xsl:value-of select="@name"/></code>
    </dt>
    <dd xmlns="http://www.w3.org/1999/xhtml">
        <xsl:variable name="name" select="@name"/>
        <xsl:choose>
          <xsl:when test="tp:docstring">
            <xsl:apply-templates select="tp:docstring"/>
          </xsl:when>
          <xsl:when test="//tp:errors/tp:error[concat(../@namespace, '.', translate(@name, ' ', ''))=$name]/tp:docstring">
            <xsl:apply-templates select="//tp:errors/tp:error[concat(../@namespace, '.', translate(@name, ' ', ''))=$name]/tp:docstring"/> <em xmlns="http://www.w3.org/1999/xhtml">(generic description)</em>
          </xsl:when>
          <xsl:otherwise>
            (Undocumented.)
          </xsl:otherwise>
        </xsl:choose>
    </dd>
  </xsl:template>

  <xsl:template match="signal">

    <xsl:call-template name="binding-name-check"/>

    <xsl:if test="not(parent::interface)">
      <xsl:message terminate="yes">
        <xsl:text>ERR: signal </xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text> does not have an interface as parent&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:if test="not(@name) or @name = ''">
      <xsl:message terminate="yes">
        <xsl:text>ERR: missing @name on a signal of </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text>&#10;</xsl:text>
      </xsl:message>
    </xsl:if>

    <xsl:for-each select="arg">
      <xsl:if test="not(@type) or @type = ''">
        <xsl:message terminate="yes">
          <xsl:text>ERR: an arg of signal </xsl:text>
          <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
          <xsl:text> has no type</xsl:text>
        </xsl:message>
      </xsl:if>
      <xsl:if test="not(@name) or @name = ''">
        <xsl:message terminate="yes">
          <xsl:text>ERR: an arg of signal </xsl:text>
          <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
          <xsl:text> has no name</xsl:text>
        </xsl:message>
      </xsl:if>
      <xsl:choose>
        <xsl:when test="not(@direction)"/>
        <xsl:when test="@direction='in'">
          <xsl:message terminate="no">
            <xsl:text>INFO: an arg of signal </xsl:text>
            <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
            <xsl:text> has unnecessary direction 'in'</xsl:text>
          </xsl:message>
        </xsl:when>
        <xsl:otherwise>
          <xsl:message terminate="yes">
            <xsl:text>ERR: an arg of signal </xsl:text>
            <xsl:value-of select="concat(../../@name, '.', ../@name)"/>
            <xsl:text> has direction other than 'in'</xsl:text>
          </xsl:message>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>

    <div xmlns="http://www.w3.org/1999/xhtml" class="signal">
      <h3 xmlns="http://www.w3.org/1999/xhtml">
        <a name="{concat(../@name, concat('.', @name))}">
          <xsl:value-of select="@name"/>
        </a> (
        <xsl:for-each xmlns="" select="arg">
          <xsl:value-of select="@type"/>: <xsl:value-of select="@name"/>
          <xsl:if test="position() != last()">, </xsl:if>
        </xsl:for-each>
        )</h3>

      <div xmlns="http://www.w3.org/1999/xhtml" class="docstring">
        <xsl:apply-templates select="tp:docstring"/>
        <xsl:apply-templates select="tp:added"/>
        <xsl:apply-templates select="tp:changed"/>
        <xsl:apply-templates select="tp:deprecated"/>
      </div>

      <xsl:if test="arg">
        <div xmlns="http://www.w3.org/1999/xhtml">
          <h4>Parameters</h4>
          <dl xmlns="http://www.w3.org/1999/xhtml">
            <xsl:apply-templates select="arg" mode="parameters-in-docstring"/>
          </dl>
        </div>
      </xsl:if>
    </div>
  </xsl:template>

  <xsl:output method="xml" indent="no" encoding="ascii"
    omit-xml-declaration="yes"
    doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"
    doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN" />

  <xsl:template match="/tp:spec">
    <html xmlns="http://www.w3.org/1999/xhtml">
      <head>
        <title>
          <xsl:value-of select="tp:title"/>
          <xsl:if test="tp:version">
            <xsl:text> version </xsl:text>
            <xsl:value-of select="tp:version"/>
          </xsl:if>
        </title>
        <style type="text/css">

          body {
            font-family: sans-serif;
            margin: 2em;
            height: 100%;
            font-size: 1.2em;
          }
          h1 {
            padding-top: 5px;
            padding-bottom: 5px;
            font-size: 1.6em;
            background: #dadae2;
          }
          h2 {
            font-size: 1.3em;
          }
          h3 {
            font-size: 1.2em;
          }
          a:link, a:visited, a:link:hover, a:visited:hover {
            font-weight: bold;
          }
          .topbox {
            padding-top: 10px;
            padding-left: 10px;
            border-bottom: black solid 1px;
            padding-bottom: 10px;
            background: #dadae2;
            font-size: 2em;
            font-weight: bold;
            color: #5c5c5c;
          }
          .topnavbox {
            padding-left: 10px;
            padding-top: 5px;
            padding-bottom: 5px;
            background: #abacba;
            border-bottom: black solid 1px;
            font-size: 1.2em;
          }
          .topnavbox a{
            color: black;
            font-weight: normal;
          }
          .sidebar {
            float: left;
            /* width:9em;
            border-right:#abacba solid 1px;
            border-left: #abacba solid 1px;
            height:100%; */
            border: #abacba solid 1px;
            padding-left: 10px;
            margin-left: 10px;
            padding-right: 10px;
            margin-right: 10px;
            color: #5d5d5d;
            background: #dadae2;
          }
          .sidebar a {
            text-decoration: none;
            border-bottom: #e29625 dotted 1px;
            color: #e29625;
            font-weight: normal;
          }
          .sidebar h1 {
            font-size: 1.2em;
            color: black;
          }
          .sidebar ul {
            padding-left: 25px;
            padding-bottom: 10px;
            border-bottom: #abacba solid 1px;
          }
          .sidebar li {
            padding-top: 2px;
            padding-bottom: 2px;
          }
          .sidebar h2 {
            font-style:italic;
            font-size: 0.81em;
            padding-left: 5px;
            padding-right: 5px;
            font-weight: normal;
          }
          .date {
            font-size: 0.6em;
            float: right;
            font-style: italic;
          }
          .method, .signal, .property {
            margin-left: 1em;
            margin-right: 4em;
          }
          .rationale {
            font-style: italic;
            border-left: 0.25em solid #808080;
            padding-left: 0.5em;
          }

          .added {
            color: #006600;
            background: #ffffff;
          }
          .deprecated {
            color: #ff0000;
            background: #ffffff;
          }
          table, tr, td, th {
            border: 1px solid #666;
          }

        </style>
      </head>
      <body>
        <h1 class="topbox">
          <xsl:value-of select="tp:title" />
        </h1>
        <xsl:if test="tp:version">
          <h2>Version <xsl:value-of select="string(tp:version)"/></h2>
        </xsl:if>
        <xsl:apply-templates select="tp:copyright"/>
        <xsl:apply-templates select="tp:license"/>
        <xsl:apply-templates select="tp:docstring"/>

        <h2>Interfaces</h2>
        <ul>
          <xsl:for-each select="//node/interface">
            <li><code><a href="#{@name}"><xsl:value-of select="@name"/></a></code></li>
          </xsl:for-each>
        </ul>

        <xsl:apply-templates select="//node"/>
        <xsl:apply-templates select="tp:generic-types"/>
        <xsl:apply-templates select="tp:errors"/>

        <h1>Index</h1>
        <h2>Index of interfaces</h2>
        <ul>
          <xsl:for-each select="//node/interface">
            <li><code><a href="#{@name}"><xsl:value-of select="@name"/></a></code></li>
          </xsl:for-each>
        </ul>
        <h2>Index of types</h2>
        <ul>
          <xsl:for-each select="//tp:simple-type | //tp:enum | //tp:flags | //tp:mapping | //tp:struct | //tp:external-type">
            <xsl:sort select="@name"/>
            <li>
              <code>
                <a href="#type-{@name}">
                  <xsl:value-of select="@name"/>
                </a>
              </code>
              <xsl:apply-templates mode="in-index" select="."/>
            </li>
          </xsl:for-each>
        </ul>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="node">
      <xsl:apply-templates />
  </xsl:template>

  <xsl:template match="text()">
    <xsl:if test="normalize-space(.) != ''">
      <xsl:message terminate="yes">
        <xsl:text>Stray text: {{{</xsl:text>
        <xsl:value-of select="." />
        <xsl:text>}}}&#10;</xsl:text>
      </xsl:message>
    </xsl:if>
  </xsl:template>

  <xsl:template match="*">
      <xsl:message terminate="yes">
         <xsl:text>Unrecognised element: {</xsl:text>
         <xsl:value-of select="namespace-uri(.)" />
         <xsl:text>}</xsl:text>
         <xsl:value-of select="local-name(.)" />
         <xsl:text>&#10;</xsl:text>
      </xsl:message>
  </xsl:template>
</xsl:stylesheet>

<!-- vim:set sw=2 sts=2 et: -->
