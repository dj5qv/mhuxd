
<?cs include:"serial_cfg.cs" ?>


<?cs if:mhuxd.run.keyer[mhuxd.webui.session.unit].flags.has.fsk1 == 1 ?>
<?cs call:sectionheader("FSK 1", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:serial_cfg("fsk1") ?>
<br><br>
<?cs /if ?>

<?cs if:mhuxd.run.keyer[mhuxd.webui.session.unit].flags.has.fsk2 == 1 ?>
<?cs call:sectionheader("FSK 2", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:serial_cfg("fsk2") ?>
<br><br>
<?cs /if ?>


