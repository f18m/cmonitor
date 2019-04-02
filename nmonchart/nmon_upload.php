<?php
$target_dir = "/webpages/docs/nmon_upload/";
$target_file = $target_dir . basename($_FILES["fileToUpload"]["name"]);
$uploadOk = 1;
$imageFileType = pathinfo($target_file,PATHINFO_EXTENSION);

echo"Processing your upload\n<ol>";
echo "<li>name - " . $_FILES['fileToUpload']['name'] . "\n";
echo "<li>size - " . $_FILES['fileToUpload']['size'] . "\n";
echo "<li>type - " . $_FILES['fileToUpload']['type'] . "\n"; 
echo "<li>tmp_name - " . $_FILES['fileToUpload']['tmp_name'] . "\n";
echo "<li>file extention - " . $imageFileType . "\n";
echo "</ul><p>\n";

// Check if image file is a actual image or fake image
//if(isset($_POST["submit"])) {
//    $check = getimagesize($_FILES["fileToUpload"]["tmp_name"]);
//    if($check !== false) {
//        echo "File is an image - " . $check["mime"] . ".";
//        echo "File is an image - " . $check["mime"] . ".";
//        echo "Write to directory - " . $target_dir . ".";
//        echo "Write file  - " . $target_file . ".";
//        $uploadOk = 1;
//    } else {
//        echo "File is not an image.";
//        $uploadOk = 0;
//    }
//}
// Check if file already exists
//if (file_exists($target_file)) {
    //echo "Sorry, file already exists.";
    //$uploadOk = 0;
//}

// Check file size
if ($_FILES["fileToUpload"]["size"] > 11000000) {
    echo "Sorry, your file is too large.";
    $uploadOk = 0;
}
// Allow certain file formats
if($imageFileType != "nmon"  && $imageFileType != "gif" ) {
    echo "Sorry, only .nmon are allowed.";
    $uploadOk = 0;
}
// Check if $uploadOk is set to 0 by an error
if ($uploadOk == 0) {
    echo "Sorry, your file was not uploaded.";
// if everything is ok, try to upload file
} else {
    if (move_uploaded_file($_FILES["fileToUpload"]["tmp_name"], $target_file)) {
        echo "<li>The file ". basename( $_FILES["fileToUpload"]["name"]). " has been uploaded.";
    } else {
        echo "Sorry, there was an error uploading your file.";
    }
}
echo "</ol>";
echo "<p>";
echo "<ul>";
echo '<li><a href="http://w3.aixncc.uk.ibm.com/nmon_upload.html">Upload another nmon data file</a>';
echo '<li><a href="http://w3.aixncc.uk.ibm.com/nmonchart/index.html">See your graphs</a> after 60 seconds';
echo "</ul>";
?> 
