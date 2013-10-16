<?cs def:hidden(name, value) ?>
<input type="hidden" name="<?cs var:name ?>" value="<?cs var:value ?>">
<?cs /def ?>
<?cs def:opt_bool_basic(optname, optvalue) ?>
    <?cs if:mhuxd.webui.session.Edit[chan] ?>
    <input type="hidden" name="<?cs var:optname ?>" value="0">
    <input type="checkbox"
	   name="<?cs var:optname ?>"
	   value="1"
	   <?cs if:optvalue=="1" ?>checked<?cs /if ?> >
    <?cs else ?>
    <?cs if:optvalue == 1 ?>
    <img src="/static/checkmark.gif" alt="true">
    <?cs else ?>
    <img src="/static/nocheckmark.gif" alt="false">
    <?cs /if ?>
    <?cs /if ?>
<?cs /def ?>
<?cs def:opt_bool(display, optname, optvalue) ?>
<tr>
  <td class="titlesettingscell" align="right"><?cs var:display ?>:</td>
  <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
  <td class="contentsettingscell">
    <?cs call:opt_bool_basic(optname, optvalue) ?>
  </td>
</tr>
<?cs /def ?>
<?cs def:opt_number_html4(display, optname, optvalue) ?>
<tr>
  <td class="titlesettingscell" align="right"><?cs var:display ?>:</td>
  <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
  <td class="contentsettingscell">
    <?cs if:mhuxd.webui.session.Edit[chan] ?>
    <input type="hidden" name="<?cs var:optname ?>" value="0">
    <input type="text"
	   name="<?cs var:optname ?>"
	   value="<?cs var:optvalue ?>" >
    <?cs else ?>
    <?cs var:optvalue ?>
    <?cs /if ?>
  </td>
</tr>
<?cs /def ?>
<?cs def:opt_number_basic(optname, optvalue) ?>
    <?cs if:mhuxd.webui.session.Edit[chan] ?>
    <input type="hidden" name="<?cs var:optname ?>" value="0">
    <input type="text" class="foobar"
	   name="<?cs var:optname ?>"
	   value="<?cs var:optvalue ?>"
	   >

    <?cs else ?>
    <?cs var:optvalue ?>
    <?cs /if ?>
<?cs /def ?>
<?cs def:opt_number(display, optname, optvalue) ?>
<tr>
  <td class="titlesettingscell" align="right"><?cs var:display ?>:</td>
  <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
  <td class="contentsettingscell">
    <?cs call:opt_number_basic(optname, optvalue) ?>
  </td>
</tr>
<?cs /def ?>
<?cs def:opt_number_id(display, optname, optvalue, id) ?>
<tr>
  <td class="titlesettingscell" align="right"><?cs var:display ?>:</td>
  <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
  <td class="contentsettingscell">
    <?cs if:mhuxd.webui.session.Edit[chan] ?>
    <input type="hidden" name="<?cs var:optname ?>" value="0">
    <input type="text"
	   name="<?cs var:optname ?>"
	   value="<?cs var:optvalue ?>"
	   id="<?cs var:id ?>"
	   >
    <?cs else ?>
    <?cs var:optvalue ?>
    <?cs /if ?>
  </td>
</tr>
<?cs /def ?>
<?cs def:opt_select_basic(optname, optlist, optvalue) ?>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<select name="<?cs var:optname ?>" >
  <?cs each:item=optlist ?>
  <option value="<?cs var:name(item) ?>"<?cs if:name(item)==optvalue || (!optvalue && item.default=="1")?> selected<?cs /if ?>><?cs var:item.display ?></option>
  <?cs /each ?>
</select>
<?cs else ?>
<?cs var:optlist[optvalue].display ?>
<?cs /if ?>
<?cs /def ?>
<?cs def:opt_select(display, optname, optlist, optvalue) ?>
<tr>
  <td class="titlesettingscell" align="right"><?cs var:display ?>:</td>
  <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
  <td class="contentsettingscell">
    <?cs call:opt_select_basic(optname, optlist, optvalue) ?>
  </td>
</tr>
<?cs /def ?>
<?cs def:keyer_status_icon(unit) ?>
  <?cs if:mhuxd.run.keyer[unit].info.status == "ONLINE" ?>
  <img src="/static/Green1.png" height="15" width="15">
  <?cs elif:mhuxd.run.keyer[unit].info.status == "OFFLINE" ?>
  <img src="/static/Yellow.png" height="15" width="15">
  <?cs else ?>
  <img src="/static/Red1.png" height="15" width="15">
  <?cs /if ?>
<?cs /def ?>
