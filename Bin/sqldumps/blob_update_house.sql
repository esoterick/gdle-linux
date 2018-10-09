CREATE DEFINER=`root`@`localhost` PROCEDURE `blob_update_house`(
    IN houseId INT(11),
    IN houseData LONGBLOB
)
BEGIN

INSERT INTO globals (house_id, DATA)
    VALUES (houseId, houseData)
    ON DUPLICATE KEY UPDATE
        DATA=houseData;

END