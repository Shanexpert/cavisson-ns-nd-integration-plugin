
TR=$1

cd work/bin

> /tmp/gui3cmd.txt
echo "Output of command nsu_get_errors P" >> /tmp/gui3cmd.txt
nsu_get_errors P >> /tmp/gui3cmd.txt
echo "Output of command nsu_get_errors S" >> /tmp/gui3cmd.txt
nsu_get_errors S >> /tmp/gui3cmd.txt
echo "Output of command nsu_get_errors U" >> /tmp/gui3cmd.txt
nsu_get_errors U >> /tmp/gui3cmd.txt
echo "Output of command nsu_get_errors T" >> /tmp/gui3cmd.txt
nsu_get_errors T >> /tmp/gui3cmd.txt

echo "Output of command nsi_get_test_runs " >> /tmp/gui3cmd.txt
nsi_get_test_runs  >> /tmp/gui3cmd.txt

echo "Output of command nsi_get_3x " >> /tmp/gui3cmd.txt
nsi_get_3x  >> /tmp/gui3cmd.txt

echo "Output of command nsi_get_3xx " >> /tmp/gui3cmd.txt
nsi_get_3xx  >> /tmp/gui3cmd.txt

echo "Output of command nsi_get_objects $TR P" >> /tmp/gui3cmd.txt
nsi_get_objects $TR P >> /tmp/gui3cmd.txt
echo "Output of command nsi_get_object $TR S" >> /tmp/gui3cmd.txt
nsi_get_objects $TR S >> /tmp/gui3cmd.txt
echo "Output of command nsi_get_objects $TR U" >> /tmp/gui3cmd.txt
nsi_get_objects $TR U >> /tmp/gui3cmd.txt
echo "Output of command nsi_get_objects $TR T" >> /tmp/gui3cmd.txt
nsi_get_objects $TR T >> /tmp/gui3cmd.txt

echo "Output of command nsi_get_location $TR" >> /tmp/gui3cmd.txt
nsi_get_location $TR >> /tmp/gui3cmd.txt

echo "Output of command nsi_get_access $TR" >> /tmp/gui3cmd.txt
nsi_get_access $TR >> /tmp/gui3cmd.txt

echo "Output of command nsi_get_4x " >> /tmp/gui3cmd.txt
nsi_get_4x >> /tmp/gui3cmd.txt

echo "Output of command nsi_get_8x " >> /tmp/gui3cmd.txt
nsi_get_8x >> /tmp/gui3cmd.txt
