# Backup/Restore Service Overview

The *backup/restore service* provides two pieces of functionality:

1.  Backup
2.  Restore

## Backup

When configuration files are created or modified, other services will inform
the BackupRestore Service.  On receipt of the notification, a timer is created
with a random number between 1-12.  That number represents the hours to delay 
before starting a backup job.  The idea here is to reduce the number of backups
created per day.

The backup job is comprised of the following steps:
1. Create a tarball of all configuration files and the version file
2. Encrypt the tarball (*optional*)
3. Upload the encrypted tarball to the cloud
4. Send notification that the backup is complete

## Restore

Generally used during an RMA, the restore process flow is:
1. Download an encrypted backup tarball from the cloud
2. Decrypt the encrypted tarball (*optional*)
3. Unpack the decrypted tarball into a temporary directory
4. Notify each service of the temporary location
5. Restart any services that respond to the notification with a
   `RESTORED_CALLBACK_RESTART`
6. Delete/cleanup the temporary directory
7. Send notification that the restore is complete
