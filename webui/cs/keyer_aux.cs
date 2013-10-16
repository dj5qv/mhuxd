<?cs include:"serial_cfg.cs" ?>
<?cs if:mhuxd.run.keyer[mhuxd.webui.session.unit].flags.has.aux == 1 ?>
<?cs call:sectionheader("AUX", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:serial_cfg("aux") ?>
<br><br>
<?cs /if ?>


