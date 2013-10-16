

<?cs include:"serial_cfg.cs" ?>


<?cs if:mhuxd.run.keyer[mhuxd.webui.session.unit].flags.has.r1 == 1 ?>
<?cs call:sectionheader("Radio 1", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:serial_cfg("r1") ?>
<br>
<?cs /if ?>

<?cs if:mhuxd.run.keyer[mhuxd.webui.session.unit].flags.has.r2 == 1 ?>
<?cs call:sectionheader("Radio 2", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:serial_cfg("r2") ?>
<br><br>
<?cs /if ?>


