<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE eagle SYSTEM "eagle.dtd">
<eagle version="9.6.2">
<drawing>
<settings>
<setting alwaysvectorfont="no"/>
<setting verticaltext="up"/>
</settings>
<grid distance="0.1" unitdist="inch" unit="inch" style="lines" multiple="1" display="no" altdistance="0.01" altunitdist="inch" altunit="inch"/>
<layers>
<layer number="1" name="Top" color="4" fill="1" visible="no" active="no"/>
<layer number="16" name="Bottom" color="1" fill="1" visible="no" active="no"/>
<layer number="17" name="Pads" color="2" fill="1" visible="no" active="no"/>
<layer number="18" name="Vias" color="2" fill="1" visible="no" active="no"/>
<layer number="19" name="Unrouted" color="6" fill="1" visible="no" active="no"/>
<layer number="20" name="Dimension" color="24" fill="1" visible="no" active="no"/>
<layer number="21" name="tPlace" color="7" fill="1" visible="no" active="no"/>
<layer number="22" name="bPlace" color="7" fill="1" visible="no" active="no"/>
<layer number="23" name="tOrigins" color="15" fill="1" visible="no" active="no"/>
<layer number="24" name="bOrigins" color="15" fill="1" visible="no" active="no"/>
<layer number="25" name="tNames" color="7" fill="1" visible="no" active="no"/>
<layer number="26" name="bNames" color="7" fill="1" visible="no" active="no"/>
<layer number="27" name="tValues" color="7" fill="1" visible="no" active="no"/>
<layer number="28" name="bValues" color="7" fill="1" visible="no" active="no"/>
<layer number="29" name="tStop" color="7" fill="3" visible="no" active="no"/>
<layer number="30" name="bStop" color="7" fill="6" visible="no" active="no"/>
<layer number="31" name="tCream" color="7" fill="4" visible="no" active="no"/>
<layer number="32" name="bCream" color="7" fill="5" visible="no" active="no"/>
<layer number="33" name="tFinish" color="6" fill="3" visible="no" active="no"/>
<layer number="34" name="bFinish" color="6" fill="6" visible="no" active="no"/>
<layer number="35" name="tGlue" color="7" fill="4" visible="no" active="no"/>
<layer number="36" name="bGlue" color="7" fill="5" visible="no" active="no"/>
<layer number="37" name="tTest" color="7" fill="1" visible="no" active="no"/>
<layer number="38" name="bTest" color="7" fill="1" visible="no" active="no"/>
<layer number="39" name="tKeepout" color="4" fill="11" visible="no" active="no"/>
<layer number="40" name="bKeepout" color="1" fill="11" visible="no" active="no"/>
<layer number="41" name="tRestrict" color="4" fill="10" visible="no" active="no"/>
<layer number="42" name="bRestrict" color="1" fill="10" visible="no" active="no"/>
<layer number="43" name="vRestrict" color="2" fill="10" visible="no" active="no"/>
<layer number="44" name="Drills" color="7" fill="1" visible="no" active="no"/>
<layer number="45" name="Holes" color="7" fill="1" visible="no" active="no"/>
<layer number="46" name="Milling" color="3" fill="1" visible="no" active="no"/>
<layer number="47" name="Measures" color="7" fill="1" visible="no" active="no"/>
<layer number="48" name="Document" color="7" fill="1" visible="no" active="no"/>
<layer number="49" name="Reference" color="7" fill="1" visible="no" active="no"/>
<layer number="51" name="tDocu" color="7" fill="1" visible="no" active="no"/>
<layer number="52" name="bDocu" color="7" fill="1" visible="no" active="no"/>
<layer number="88" name="SimResults" color="9" fill="1" visible="yes" active="yes"/>
<layer number="89" name="SimProbes" color="9" fill="1" visible="yes" active="yes"/>
<layer number="90" name="Modules" color="5" fill="1" visible="yes" active="yes"/>
<layer number="91" name="Nets" color="2" fill="1" visible="yes" active="yes"/>
<layer number="92" name="Busses" color="1" fill="1" visible="yes" active="yes"/>
<layer number="93" name="Pins" color="2" fill="1" visible="no" active="yes"/>
<layer number="94" name="Symbols" color="4" fill="1" visible="yes" active="yes"/>
<layer number="95" name="Names" color="7" fill="1" visible="yes" active="yes"/>
<layer number="96" name="Values" color="7" fill="1" visible="yes" active="yes"/>
<layer number="97" name="Info" color="7" fill="1" visible="yes" active="yes"/>
<layer number="98" name="Guide" color="6" fill="1" visible="yes" active="yes"/>
<layer number="250" name="Descript" color="3" fill="1" visible="no" active="yes"/>
<layer number="254" name="OrgLBR" color="13" fill="1" visible="no" active="yes"/>
</layers>
<schematic xreflabel="%F%N/%S.%C%R" xrefpart="/%S.%C%R">
<libraries>
<library name="frames" urn="urn:adsk.eagle:library:229">
<description>&lt;b&gt;Frames for Sheet and Layout&lt;/b&gt;</description>
<packages>
</packages>
<symbols>
<symbol name="DINA4_L" urn="urn:adsk.eagle:symbol:13867/1" library_version="1">
<frame x1="0" y1="0" x2="264.16" y2="180.34" columns="4" rows="4" layer="94" border-left="no" border-top="no" border-right="no" border-bottom="no"/>
</symbol>
<symbol name="DOCFIELD" urn="urn:adsk.eagle:symbol:13864/1" library_version="1">
<wire x1="0" y1="0" x2="71.12" y2="0" width="0.1016" layer="94"/>
<wire x1="101.6" y1="15.24" x2="87.63" y2="15.24" width="0.1016" layer="94"/>
<wire x1="0" y1="0" x2="0" y2="5.08" width="0.1016" layer="94"/>
<wire x1="0" y1="5.08" x2="71.12" y2="5.08" width="0.1016" layer="94"/>
<wire x1="0" y1="5.08" x2="0" y2="15.24" width="0.1016" layer="94"/>
<wire x1="101.6" y1="15.24" x2="101.6" y2="5.08" width="0.1016" layer="94"/>
<wire x1="71.12" y1="5.08" x2="71.12" y2="0" width="0.1016" layer="94"/>
<wire x1="71.12" y1="5.08" x2="87.63" y2="5.08" width="0.1016" layer="94"/>
<wire x1="71.12" y1="0" x2="101.6" y2="0" width="0.1016" layer="94"/>
<wire x1="87.63" y1="15.24" x2="87.63" y2="5.08" width="0.1016" layer="94"/>
<wire x1="87.63" y1="15.24" x2="0" y2="15.24" width="0.1016" layer="94"/>
<wire x1="87.63" y1="5.08" x2="101.6" y2="5.08" width="0.1016" layer="94"/>
<wire x1="101.6" y1="5.08" x2="101.6" y2="0" width="0.1016" layer="94"/>
<wire x1="0" y1="15.24" x2="0" y2="22.86" width="0.1016" layer="94"/>
<wire x1="101.6" y1="35.56" x2="0" y2="35.56" width="0.1016" layer="94"/>
<wire x1="101.6" y1="35.56" x2="101.6" y2="22.86" width="0.1016" layer="94"/>
<wire x1="0" y1="22.86" x2="101.6" y2="22.86" width="0.1016" layer="94"/>
<wire x1="0" y1="22.86" x2="0" y2="35.56" width="0.1016" layer="94"/>
<wire x1="101.6" y1="22.86" x2="101.6" y2="15.24" width="0.1016" layer="94"/>
<text x="1.27" y="1.27" size="2.54" layer="94">Date:</text>
<text x="12.7" y="1.27" size="2.54" layer="94">&gt;LAST_DATE_TIME</text>
<text x="72.39" y="1.27" size="2.54" layer="94">Sheet:</text>
<text x="86.36" y="1.27" size="2.54" layer="94">&gt;SHEET</text>
<text x="88.9" y="11.43" size="2.54" layer="94">REV:</text>
<text x="1.27" y="19.05" size="2.54" layer="94">TITLE:</text>
<text x="1.27" y="11.43" size="2.54" layer="94">Document Number:</text>
<text x="17.78" y="19.05" size="2.54" layer="94">&gt;DRAWING_NAME</text>
</symbol>
</symbols>
<devicesets>
<deviceset name="DINA4_L" urn="urn:adsk.eagle:component:13919/1" prefix="FRAME" uservalue="yes" library_version="1">
<description>&lt;b&gt;FRAME&lt;/b&gt;&lt;p&gt;
DIN A4, landscape with extra doc field</description>
<gates>
<gate name="G$1" symbol="DINA4_L" x="0" y="0"/>
<gate name="G$2" symbol="DOCFIELD" x="162.56" y="0" addlevel="must"/>
</gates>
<devices>
<device name="">
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
</devicesets>
</library>
</libraries>
<attributes>
</attributes>
<variantdefs>
</variantdefs>
<classes>
<class number="0" name="default" width="0" drill="0">
</class>
</classes>
<parts>
<part name="FRAME1" library="frames" library_urn="urn:adsk.eagle:library:229" deviceset="DINA4_L" device=""/>
</parts>
<sheets>
<sheet>
<plain>
<wire x1="157.48" y1="160.02" x2="157.48" y2="53.34" width="0.1524" layer="94"/>
<wire x1="157.48" y1="53.34" x2="231.14" y2="53.34" width="0.1524" layer="94"/>
<wire x1="231.14" y1="53.34" x2="231.14" y2="160.02" width="0.1524" layer="94"/>
<wire x1="45.72" y1="116.84" x2="45.72" y2="96.52" width="0.1524" layer="94"/>
<wire x1="45.72" y1="96.52" x2="78.74" y2="96.52" width="0.1524" layer="94"/>
<wire x1="78.74" y1="96.52" x2="78.74" y2="116.84" width="0.1524" layer="94"/>
<wire x1="78.74" y1="116.84" x2="76.2" y2="116.84" width="0.1524" layer="94"/>
<wire x1="76.2" y1="116.84" x2="73.66" y2="116.84" width="0.1524" layer="94"/>
<wire x1="73.66" y1="116.84" x2="50.8" y2="116.84" width="0.1524" layer="94"/>
<wire x1="50.8" y1="116.84" x2="48.26" y2="116.84" width="0.1524" layer="94"/>
<wire x1="48.26" y1="116.84" x2="45.72" y2="116.84" width="0.1524" layer="94"/>
<wire x1="48.26" y1="116.84" x2="48.26" y2="119.38" width="0.1524" layer="94"/>
<wire x1="48.26" y1="119.38" x2="50.8" y2="119.38" width="0.1524" layer="94"/>
<wire x1="50.8" y1="119.38" x2="50.8" y2="116.84" width="0.1524" layer="94"/>
<wire x1="73.66" y1="116.84" x2="73.66" y2="119.38" width="0.1524" layer="94"/>
<wire x1="73.66" y1="119.38" x2="76.2" y2="119.38" width="0.1524" layer="94"/>
<wire x1="76.2" y1="119.38" x2="76.2" y2="116.84" width="0.1524" layer="94"/>
<text x="58.42" y="104.14" size="1.778" layer="94">Battery</text>
<text x="48.768" y="117.348" size="1.778" layer="94">+</text>
<text x="74.676" y="117.348" size="1.778" layer="94">-</text>
<wire x1="99.06" y1="106.68" x2="99.06" y2="88.9" width="0.1524" layer="94"/>
<wire x1="99.06" y1="88.9" x2="106.68" y2="88.9" width="0.1524" layer="94"/>
<wire x1="106.68" y1="88.9" x2="106.68" y2="106.68" width="0.1524" layer="94"/>
<wire x1="106.68" y1="106.68" x2="99.06" y2="106.68" width="0.1524" layer="94"/>
<text x="109.22" y="99.06" size="1.778" layer="95">Shunt
200A / 0.75mV</text>
<wire x1="53.34" y1="129.54" x2="60.96" y2="129.54" width="0.1524" layer="94"/>
<wire x1="54.61" y1="130.302" x2="54.61" y2="128.778" width="0.1524" layer="94"/>
<wire x1="54.61" y1="128.778" x2="59.69" y2="128.778" width="0.1524" layer="94"/>
<wire x1="59.69" y1="128.778" x2="59.69" y2="130.302" width="0.1524" layer="94"/>
<wire x1="59.69" y1="130.302" x2="54.61" y2="130.302" width="0.1524" layer="94"/>
<wire x1="157.48" y1="160.02" x2="231.14" y2="160.02" width="0.1524" layer="94"/>
<text x="54.864" y="132.588" size="1.778" layer="95">F1</text>
<text x="54.864" y="130.556" size="1.778" layer="96">3A</text>
<text x="185.42" y="119.38" size="1.778" layer="94">BatteryMonitor</text>
</plain>
<instances>
<instance part="FRAME1" gate="G$1" x="0" y="0" smashed="yes"/>
<instance part="FRAME1" gate="G$2" x="162.56" y="0" smashed="yes">
<attribute name="LAST_DATE_TIME" x="175.26" y="1.27" size="2.54" layer="94"/>
<attribute name="SHEET" x="248.92" y="1.27" size="2.54" layer="94"/>
<attribute name="DRAWING_NAME" x="180.34" y="19.05" size="2.54" layer="94"/>
</instance>
</instances>
<busses>
</busses>
<nets>
<net name="IN+" class="0">
<segment>
<wire x1="74.676" y1="119.38" x2="74.676" y2="124.46" width="0.1524" layer="91"/>
<wire x1="74.676" y1="124.46" x2="103.124" y2="124.46" width="0.1524" layer="91"/>
<wire x1="103.124" y1="124.46" x2="103.124" y2="106.426" width="0.1524" layer="91"/>
<wire x1="103.124" y1="124.46" x2="157.48" y2="124.46" width="0.1524" layer="91"/>
<junction x="103.124" y="124.46"/>
<label x="149.86" y="124.46" size="1.778" layer="95"/>
</segment>
</net>
<net name="IN-" class="0">
<segment>
<wire x1="102.87" y1="88.9" x2="102.87" y2="81.788" width="0.1524" layer="91"/>
<wire x1="102.87" y1="81.788" x2="102.87" y2="61.722" width="0.1524" layer="91"/>
<wire x1="102.87" y1="61.722" x2="102.616" y2="61.722" width="0.1524" layer="91"/>
<wire x1="102.87" y1="81.788" x2="157.48" y2="81.788" width="0.1524" layer="91"/>
<junction x="102.87" y="81.788"/>
<label x="148.082" y="82.296" size="1.778" layer="95"/>
</segment>
</net>
<net name="N$3" class="0">
<segment>
<wire x1="49.53" y1="119.38" x2="49.53" y2="129.54" width="0.1524" layer="91"/>
<wire x1="49.53" y1="129.54" x2="35.56" y2="129.54" width="0.1524" layer="91"/>
<wire x1="35.56" y1="129.54" x2="35.56" y2="58.42" width="0.1524" layer="91"/>
<wire x1="49.53" y1="129.54" x2="53.34" y2="129.54" width="0.1524" layer="91"/>
<junction x="49.53" y="129.54"/>
</segment>
</net>
<net name="VBS" class="0">
<segment>
<wire x1="60.96" y1="129.54" x2="157.48" y2="129.54" width="0.1524" layer="91"/>
<label x="149.86" y="129.54" size="1.778" layer="95"/>
</segment>
</net>
</nets>
</sheet>
</sheets>
</schematic>
</drawing>
<compatibility>
<note version="6.3" minversion="6.2.2" severity="warning">
Since Version 6.2.2 text objects can contain more than one line,
which will not be processed correctly with this version.
</note>
<note version="8.2" severity="warning">
Since Version 8.2, EAGLE supports online libraries. The ids
of those online libraries will not be understood (or retained)
with this version.
</note>
<note version="8.3" severity="warning">
Since Version 8.3, EAGLE supports URNs for individual library
assets (packages, symbols, and devices). The URNs of those assets
will not be understood (or retained) with this version.
</note>
</compatibility>
</eagle>
