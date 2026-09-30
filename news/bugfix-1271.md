`http()`: fixed batches exceeding `batch-bytes()` by the message that did not fit, that message now starts the next
batch. A single message larger than `batch-bytes()` is still sent alone.
