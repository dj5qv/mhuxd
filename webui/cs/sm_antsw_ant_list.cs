<?cs def:sm_antsw_ant_list1(unit, chan) ?>

<!-- BASE PAGE -->


<?cs call:sectionheader("Antenna List", "foobar_help") ?>

&nbsp;&nbsp;<br>

<table class="sectiontable" cellpadding="3" cellspacing="0" width="100%">
  <tbody>
    <tr>
      <td class="titlelistcell">&nbsp;</td>
      <td class="titlelistcell">ID</td>
      <td class="titlelistcell">Label</td>
      <td class="titlelistcell">Name</td>
      <td class="titlelistcell">SteppIR</td>
      <td class="titlelistcell">RX-Only</td>
      <td class="titlelistcell">PA Ant Nr</td>
      <td class="titlelistcell">Rotator</td>
      <td class="titlelistcell">Rotator Offset</td>

      <?cs each:item = mhuxd.keyer[unit].sm.output ?>
      <?cs if:mhuxd.keyer[unit].sm.output[name(item)] == 0 ?>
      <td class="titlelistcell"><?cs var:name(item) ?></td>
      <?cs /if ?>
      <?cs /each ?>

    </tr>

    <?cs each:item = mhuxd.keyer[unit].sm.ant ?>

    <tr class="contentlistrow2">

      <?cs if:mhuxd.webui.session.Edit[chan] ?>
      <td class="contentlistcell" width="19" align="center">&nbsp;&nbsp;</td>
      <?cs call:hidden("modify.mhuxd.keyer."+unit+".sm.ant."+name(item)+".id", mhuxd.keyer[unit].sm.ant[name(item)].id) ?>
      <?cs else ?>
      <td class="radiolistcell2" width="19" align="center">
	<input type="checkbox" name="modify.mhuxd.keyer.<?cs var:unit ?>.sm.ant.<?cs var:name(item) ?>" value="1" > 
      </td>
      <?cs /if ?>

      <td class="contentlistcell"><?cs var:name(item) ?></td>

      <td class="contentlistcell"><?cs call:string_basic(
				       "modify.mhuxd.keyer."+unit+".sm.ant."+name(item)+".label", 
				       mhuxd.keyer[unit].sm.ant[name(item)].label, 5 ) ?></td>

      <td class="contentlistcell"><?cs call:string_basic(
				       "modify.mhuxd.keyer."+unit+".sm.ant."+name(item)+".name", 
				       mhuxd.keyer[unit].sm.ant[name(item)].name, 10 ) ?> </td>

      <td class="contentlistcell"><?cs call:opt_bool_basic(
				       "modify.mhuxd.keyer."+unit+".sm.ant."+name(item)+".steppir", 
				       mhuxd.keyer[unit].sm.ant[name(item)].steppir ) ?></td>

      <td class="contentlistcell"><?cs call:opt_bool_basic(
				       "modify.mhuxd.keyer."+unit+".sm.ant."+name(item)+".rxonly", 
				       mhuxd.keyer[unit].sm.ant[name(item)].rxonly ) ?></td>

      <td class="contentlistcell"><?cs call:opt_number_basic(
				       "modify.mhuxd.keyer."+unit+".sm.ant."+name(item)+".pa_ant_number", 
				       mhuxd.keyer[unit].sm.ant[name(item)].pa_ant_number ) ?> </td>

      <td class="contentlistcell"><?cs call:opt_bool_basic(
				       "modify.mhuxd.keyer."+unit+".sm.ant."+name(item)+".rotator", 
				       mhuxd.keyer[unit].sm.ant[name(item)].rotator ) ?></td>

      <td class="contentlistcell"><?cs call:opt_number_basic(
				       "modify.mhuxd.keyer."+unit+".sm.ant."+name(item)+".rotator_offset", 
				       mhuxd.keyer[unit].sm.ant[name(item)].rotator_offset ) ?> </td>


      <?cs each:subitem = mhuxd.keyer[unit].sm.output ?>
      <?cs if:mhuxd.keyer[unit].sm.output[name(subitem)] == 0 ?>
      <td class="contentlistcell"><?cs call:opt_bool_basic(
				       "modify.mhuxd.keyer."+unit+".sm.ant."+name(item)+".output."+name(subitem), 
				       mhuxd.keyer[unit].sm.ant[name(item)].output[name(subitem)] ) ?></td>
      <?cs /if ?>
      <?cs /each ?>


    </tr>

    <?cs /each ?>

    <!-- Add a new one -->
    <?cs if:mhuxd.webui.session.Add[chan] ?>
    <tr class="contentlistrow2">
      <td class="contentlistcell" width="19" align="center">&nbsp;&nbsp;</td>
      <td class="contentlistcell" width="19" align="center">&nbsp;&nbsp;</td>
      <td class="contentlistcell"><?cs call:string_rw(
				       "set.mhuxd.keyer."+unit+".sm.ant.0.label", 
				       "", 5 ) ?></td>

      <td class="contentlistcell"><?cs call:string_rw(
				       "set.mhuxd.keyer."+unit+".sm.ant.0.name", 
				       "", 10 ) ?> </td>

      <td class="contentlistcell"><?cs call:bool_rw(
				       "set.mhuxd.keyer."+unit+".sm.ant.0.steppir", 
				       0 ) ?></td>

      <td class="contentlistcell"><?cs call:bool_rw(
				       "set.mhuxd.keyer."+unit+".sm.ant.0.rxonly", 
				       0 ) ?></td>

      <td class="contentlistcell"><?cs call:number_rw(
				       "set.mhuxd.keyer."+unit+".sm.ant.0.pa_ant_number", 
				       0 ) ?> </td>

      <td class="contentlistcell"><?cs call:bool_rw(
				       "set.mhuxd.keyer."+unit+".sm.ant.0.rotator", 
				       0 ) ?></td>

      <td class="contentlistcell"><?cs call:number_rw(
				       "set.mhuxd.keyer."+unit+".sm.ant.0.rotator_offset", 
				       mhuxd.keyer[unit].sm.ant[name(item)].rotator_offset ) ?> </td>
      <?cs each:subitem = mhuxd.keyer[unit].sm.output ?>
      <?cs if:mhuxd.keyer[unit].sm.output[name(subitem)] == 0 ?>
      <td class="contentlistcell"><?cs call:bool_rw(
				       "set.mhuxd.keyer."+unit+".sm.ant.0.output."+name(subitem), 
				       0 ) ?></td>
      <?cs /if ?>
      <?cs /each ?>
    </tr>
    <?cs /if ?>
    <!-- /Add new one -->

  </tbody>
</table>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>


<?cs /def ?>

<?cs def:sm_antsw_ant_list(unit, chan) ?>
<?cs call:sm_antsw_ant_list1(mhuxd.webui.session.unit, chan) ?>

<?cs if:mhuxd.webui.session.Add[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs elseif:mhuxd.webui.session.Edit[chan] ?>
<input name="Modify" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>

<input name="Add.<?cs var:chan ?>" value="Add" type="submit">
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<input name="Remove" value="Remove" type="submit">
<?cs /if ?>
<?cs /def ?>

<?cs call:sm_antsw_ant_list(unit, "sm_antsw_ant_list") ?>
